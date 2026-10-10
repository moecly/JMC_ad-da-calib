#include "calib/CalibProc.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Application.h"
#include "CuteLogger/CuteLogger.h"
#include "FWConfig.h"
#include "SysDelay.h"
#include "at32f425_gpio.h"
#include "at32f425_wk_config.h"
#include "calib/CalibLink.h"
#include "calib/CalibOps.h"
#include "calib/CalibStore.h"
#include "calib/PolyFit.h"
#include "Rtt.h"

#if defined(CONFIG_CALIB_ROLE_CHK) && !defined(CONFIG_DEVICE_DA_4CH)
#error "CALIB_ROLE=CHK drives and measures DA, only valid for DA_4CH"
#endif

#if defined(CONFIG_CALIB_ROLE_MANUAL) && !defined(CONFIG_DEVICE_AD_4CH) && !defined(CONFIG_DEVICE_DA_4CH)
#error "CALIB_ROLE=MANUAL only valid for AD_4CH or DA_4CH"
#endif

#if defined(CONFIG_DEVICE_AD_4CH)
constexpr const char* kBoardDir = "ad";
constexpr bool kManualAd = true;
#elif defined(CONFIG_DEVICE_DA_4CH)
constexpr const char* kBoardDir = "da";
constexpr bool kManualAd = false;
#else
constexpr const char* kBoardDir = "?";
constexpr bool kManualAd = false;
#endif

// 与 Application::VersionPrint 同格式，供上位机正则匹配板名/版本
void PrintDevice() {
    char m[64];
    if (CONFIG_MINOR_VERSION < 10)
        snprintf(m, sizeof(m), "Device: %s, App Version: v%d.0%d", CONFIG_DEVICE_NAME, CONFIG_MAJOR_VERSION,
                 CONFIG_MINOR_VERSION);
    else
        snprintf(m, sizeof(m), "Device: %s, App Version: v%d.%d", CONFIG_DEVICE_NAME, CONFIG_MAJOR_VERSION,
                 CONFIG_MINOR_VERSION);
    LogInfo << m;
}

namespace {

constexpr int kGridN = 67;
const uint16_t kGrid[kGridN] = {0,     1000,  2000,  3000,  4000,  5000,  6000,  7000,  8000,  9000,  10000, 11000,
                                12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000, 20000, 21000, 22000, 23000,
                                24000, 25000, 26000, 27000, 28000, 29000, 30000, 31000, 32000, 33000, 34000, 35000,
                                36000, 37000, 38000, 39000, 40000, 41000, 42000, 43000, 44000, 45000, 46000, 47000,
                                48000, 49000, 50000, 51000, 52000, 53000, 54000, 55000, 56000, 57000, 58000, 59000,
                                60000, 61000, 62000, 63000, 64000, 65000, 65535};

#if defined(CALIB_FIXED_IDEAL)
// 驻留自检：把整个扫描点都钉在同一个 ideal 上，分离「施加不稳」与「采集不稳」
uint16_t kGridLive[kGridN];
#else
const uint16_t* const kGridLive = kGrid;
#endif

// 建压等待；CALIB_SETTLE_MS 可覆盖，用于定位 DA/RC 建立时间不足
#if defined(CALIB_SETTLE_MS)
constexpr uint32_t kSettleMs = CALIB_SETTLE_MS;
#else
constexpr uint32_t kSettleMs = 200;
#endif
// 稳定判据：DA 建压等待后采 kStableCount 次，窗内极差 > kStableCode 只打 unstable 标记，不中断
constexpr uint32_t kSampleIntervalMs = 5;
constexpr int kStableCount = 16;
constexpr int kStableCode = 20;
constexpr uint32_t kReplyTimeoutMs = 20;
constexpr int kRetryMax = 3;
constexpr uint32_t kPingIntervalMs = 100;
constexpr int kPingMax = 20;
constexpr double kResidualLimitCode = 1000.0;
constexpr double kCheckGateCode = 16.0;
// 参考端可信通道：硬件与标定实测均为 ch0，内建曲线 code_to_code() 只对 ch0 成立，
// 无校准记录时非 ch0 通道不做换算（原值直通）
constexpr uint8_t kRefCh = 0;

CalibRecord s_rec;
#if defined(CONFIG_CALIB_ROLE_CHK)
constexpr bool kCheckMode = true;
#else
constexpr bool kCheckMode = false;
#endif

enum State { S_IDLE, S_PING, S_SWEEP, S_FIT, S_CHECK, S_DUMP, S_SAVE, S_VERIFY, S_DONE, S_FAIL };
enum Phase { PH_SEND, PH_WAIT, PH_SETTLE };

float s_samples[4][kGridN];
float s_verify[4][kGridN];
double s_verMax[4];
double s_verSumSq[4];
uint16_t s_verMaxAt[4];
int s_verifyIdx;
bool s_verifyReported;
int s_dumpLine;
int s_dumpWait;
double s_gridD[kGridN];
double s_coeff[4][POLY_COEFF_NUM];
double s_chkMax[4];
uint16_t s_chkMaxAt[4];

State s_state = S_IDLE;
Phase s_phase = PH_SEND;
int s_sweepIdx;
int s_stableCount;
uint16_t s_stableMin[4];
uint16_t s_stableMax[4];
int s_retry;
uint32_t s_waitMs;
uint32_t s_pingElapsed;
int s_pingCount;
uint32_t s_failMs;
bool s_ledOn;

// DA 方向（DA 板 DUT/CHK）：已向 REF 发 OP_READ_AD 等应答，及窗内极值（REF 回传 float ideal）
bool s_awaitReply = false;
float s_measMin[4];
float s_measMax[4];
float s_measMid[4];
float s_measWin[4][kStableCount];
// REF 的 AD 值是 32 槽均值滤波的输出：单个坏样本（SPI/通道切换毛刺）进入后会把整窗抬升
// ~5000/32≈156 码并持续 128 ms。采样间隔取 20 ms、窗长 320 ms 跨过滤波长度 + 取中值，
// 使少数被污染的样本无法改变结果（实测复检随机 ~150~260 码超差即源于此）
constexpr uint32_t kRefSampleIntervalMs = 20;
static inline float MedianWin(float* v, int n) {
    for (int i = 1; i < n; i++) {
        float key = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
    return 0.5f * (v[n / 2 - 1] + v[n / 2]);
}
// AD 方向并行：本点需驱动的 REF DA 通道去重列表与已收 ACK 数
uint8_t s_driveCh[4];
int s_driveCount = 0;
int s_ackCount = 0;

// 上位机控制的按「连线」校准配置：每个勾选的 AD 通道配一个 REF DA 源
struct CalibRun {
    uint8_t adCh;
    uint8_t daCh;
};
uint8_t s_chMask = 0x0F;
uint8_t s_refDa[4] = {0, 0, 0, 0};
CalibRun s_runs[4];
int s_runCount = 0;
bool s_paused = false;

Board& Bd() { return Application::GetInstance(); }

void LedSet(bool on) {
    if (on) {
        gpio_bits_reset(LED_RUN_GPIO_PORT, LED_RUN_PIN);
    } else {
        gpio_bits_set(LED_RUN_GPIO_PORT, LED_RUN_PIN);
    }
    s_ledOn = on;
}

void LedToggle() { LedSet(!s_ledOn); }

void EnterFail() {
    s_state = S_FAIL;
    s_failMs = 0;
}

const char* StateName() {
    if (s_paused) return "paused";
    switch (s_state) {
        case S_IDLE: return "idle";
        case S_PING: return "ping";
        case S_SWEEP: return "sweep";
        case S_FIT: return "fit";
        case S_CHECK: return "check";
        case S_DUMP: return "dump";
        case S_SAVE: return "save";
        case S_VERIFY: return "verify";
        case S_DONE: return "done";
        case S_FAIL: return "fail";
    }
    return "?";
}

void PrintState() {
    char msg[48];
    snprintf(msg, sizeof(msg), "calib: state %s", StateName());
    LogInfo << msg;
}

void ConfigFromMask() {
    s_runCount = 0;
    for (uint8_t ch = 0; ch < 4; ch++) {
        if ((s_chMask >> ch) & 1) {
            s_runs[s_runCount].adCh = ch;
            s_runs[s_runCount].daCh = s_refDa[ch];
            s_runCount++;
        }
    }
}

void ResetRunCounters() {
    s_runCount = 0;
    s_sweepIdx = 0;
    s_pingCount = 0;
    s_pingElapsed = kPingIntervalMs;
    s_retry = 0;
    s_waitMs = 0;
    s_verifyReported = false;
    s_verifyIdx = 0;
}

#if defined(CONFIG_CALIB_ROLE_REF)

void RefTick() {
    uint8_t payload[40];
    uint8_t len;
    if (CalibLinkRecv(payload, &len) != 0 || len < 1) return;

    uint8_t resp[24];
    switch (payload[0]) {
        case OP_PING:
            resp[0] = OP_PONG;
            CalibLinkSend(resp, 1);
            break;
        case OP_SET_DA:
#if defined(CONFIG_DEVICE_DA_4CH)
            if (len >= 4) {
                uint16_t ideal;
                std::memcpy(&ideal, payload + 2, 2);
                uint16_t rawToSet = Bd().CalibDaRaw(payload[1], ideal);
                Bd().CalibDaSet(payload[1], rawToSet);
                LogInfo << "calib: set_da ch=" << (unsigned)payload[1] << " ideal=" << ideal << " raw=" << rawToSet;
                resp[0] = OP_ACK_SET_DA;
            } else {
                resp[0] = OP_NACK;
            }
#else
            resp[0] = OP_NACK;
#endif
            CalibLinkSend(resp, 1);
            break;
        case OP_READ_AD:
#if defined(CONFIG_DEVICE_AD_4CH)
            resp[0] = OP_ACK_READ_AD;
            for (int ch = 0; ch < 4; ch++) {
                float v = Bd().CalibAdIdeal(ch, Bd().CalibAdRaw(ch));
                std::memcpy(resp + 1 + 4 * ch, &v, 4);
            }
            CalibLinkSend(resp, 17);
#else
            resp[0] = OP_NACK;
            CalibLinkSend(resp, 1);
#endif
            break;
        case OP_GET_INFO: {
            CalibRecord r;
            bool has = CalibStoreLoad(&r);
#if defined(CONFIG_DEVICE_AD_4CH)
            uint16_t board = 1;
#else
            uint16_t board = 2;
#endif
            uint16_t mask = has ? r.validMask : 0;
            resp[0] = OP_ACK_INFO;
            resp[1] = (uint8_t)board;
            resp[2] = (uint8_t)CONFIG_MAJOR_VERSION;
            resp[3] = (uint8_t)CONFIG_MINOR_VERSION;
            resp[4] = has ? 1 : 0;
            resp[5] = (uint8_t)POLY_DEGREE;
            resp[6] = 4;
            resp[7] = (uint8_t)(mask & 0xFF);
            resp[8] = (uint8_t)(mask >> 8);
            CalibLinkSend(resp, 9);
            break;
        }
        default:
            resp[0] = OP_NACK;
            CalibLinkSend(resp, 1);
            break;
    }
}

#elif defined(CONFIG_CALIB_ROLE_MANUAL)

constexpr int kManualMax = 16;             // 每通道最多记录点
constexpr int kManualMinPoints = 8;        // <8 点拒绝拟合（三次，4 点只是过拟合）
constexpr uint32_t kManualMinSpan = 4096;  // 拟合 x 的最小跨度（码）

uint8_t s_cmdRx[64];
int s_cmdLen = 0;

bool s_manualOn = false;
bool s_sampling = false;       // AD 采集进行中
uint16_t s_mx[4][kManualMax];  // AD: raw   ；DA: ideal
uint16_t s_my[4][kManualMax];  // AD: ideal ；DA: raw
int s_manualN[4];
bool s_manualFitted[4];
double s_mCoeff[4][POLY_COEFF_NUM];
uint16_t s_lastRaw[4];  // DA: 最近一次写入的原始码
int s_samplingCh = -1;
uint16_t s_samplingIdeal = 0;
uint32_t s_mWaitMs;
int s_mStable;
uint16_t s_mMin, s_mMax;

// x/y 方向已按板型写入 s_mx/s_my，故 PolyFit 与残差公式对两种板型同一套：
//   AD: x=raw  y=ideal → coeff 语义 ideal=f(raw) 对齐 CalibAdIdeal
//   DA: x=ideal y=raw  → coeff 语义 raw=f(ideal) 对齐 CalibDaRaw
void ManualFit() {
    char m[96];
    for (int ch = 0; ch < 4; ch++) {
        int n = s_manualN[ch];
        if (n == 0) continue;
        double xs[kManualMax];
        double ys[kManualMax];
        double xmin = 1e18;
        double xmax = -1e18;
        for (int i = 0; i < n; i++) {
            xs[i] = (double)s_mx[ch][i];
            ys[i] = (double)s_my[ch][i];
            if (xs[i] < xmin) xmin = xs[i];
            if (xs[i] > xmax) xmax = xs[i];
        }
        uint32_t span = (uint32_t)(xmax - xmin);
        if (n < kManualMinPoints || span < kManualMinSpan) {
            s_manualFitted[ch] = false;
            snprintf(m, sizeof(m), "calib: mfit reject ch=%u n=%u span=%u", (unsigned)ch, (unsigned)n, (unsigned)span);
            LogInfo << m;
            continue;
        }
        double coeff[POLY_COEFF_NUM];
        if (!PolyFit(xs, ys, n, coeff)) {
            s_manualFitted[ch] = false;
            snprintf(m, sizeof(m), "calib: mfit failed ch=%u", (unsigned)ch);
            LogError << m;
            continue;
        }
        for (int k = 0; k < POLY_COEFF_NUM; k++) s_mCoeff[ch][k] = coeff[k];
        s_manualFitted[ch] = true;
        double maxRes = 0.0;
        double sumSq = 0.0;
        int maxAt = 0;
        for (int i = 0; i < n; i++) {
            double d = PolyEval(coeff, xs[i]) - ys[i];
            sumSq += d * d;
            double ad = d < 0 ? -d : d;
            if (ad > maxRes) {
                maxRes = ad;
                maxAt = i;
            }
        }
        uint16_t idealAt = kManualAd ? s_my[ch][maxAt] : s_mx[ch][maxAt];
        snprintf(m, sizeof(m), "calib: mres ch=%u n=%u max=%.1f @ideal=%u rmse=%.1f", (unsigned)ch, (unsigned)n, maxRes,
                 (unsigned)idealAt, sqrt(sumSq / n));
        LogInfo << m;
    }
}

void ManualSave() {
    bool any = false;
    for (int ch = 0; ch < 4; ch++) {
        if (s_manualFitted[ch]) any = true;
    }
    if (!any) {
        LogError << "calib: error manual-no-fit";
        return;
    }

    CalibRecord rec;
    if (!CalibStoreLoad(&rec)) {
        rec = CalibRecord{};
        rec.validMask = 0;
    }
    rec.board = kManualAd ? 1 : 2;
    rec.degree = POLY_DEGREE;
    rec.channelCount = 4;
    for (int ch = 0; ch < 4; ch++) {
        if (!s_manualFitted[ch]) continue;
        for (int k = 0; k < POLY_COEFF_NUM; k++) rec.coeff[ch][k] = s_mCoeff[ch][k];
        rec.validMask |= (uint16_t)(1u << ch);
    }
    if (!CalibStoreSave(&rec)) {
        LogError << "calib: flash write failed";
        return;
    }
    s_rec = rec;
    LogInfo << "calib: msaved";
    {
        char m[48];
        snprintf(m, sizeof(m), "calib: mask 0x%X", (unsigned)s_rec.validMask);
        LogInfo << m;
    }

    // 单板无自动施加源，复检 = 落盘回读 + 用回读系数复算已采点，抓「写盘损坏 / 系数错位」
    CalibRecord rb;
    if (!CalibStoreLoad(&rb)) {
        LogError << "calib: mverify FAIL";
        return;
    }
    bool pass = true;
    for (int ch = 0; ch < 4; ch++) {
        if (!s_manualFitted[ch]) continue;
        int n = s_manualN[ch];
        double maxRes = 0.0;
        double sumSq = 0.0;
        int maxAt = 0;
        for (int i = 0; i < n; i++) {
            double d = PolyEval(rb.coeff[ch], (double)s_mx[ch][i]) - (double)s_my[ch][i];
            sumSq += d * d;
            double ad = d < 0 ? -d : d;
            if (ad > maxRes) {
                maxRes = ad;
                maxAt = i;
            }
        }
        uint16_t idealAt = kManualAd ? s_my[ch][maxAt] : s_mx[ch][maxAt];
        char m[96];
        snprintf(m, sizeof(m), "calib: mverify ch=%u max=%.1f @ideal=%u rmse=%.1f", (unsigned)ch, maxRes,
                 (unsigned)idealAt, sqrt(sumSq / n));
        LogInfo << m;
        if (maxRes >= kCheckGateCode) pass = false;
    }
    if (pass) {
        LogInfo << "calib: mverify PASS";
    } else {
        LogError << "calib: mverify FAIL";
    }
}

void HandleManual(const uint8_t* p, uint8_t len) {
    char m[128];
    switch (p[0]) {
        case OP_HOST_MANUAL_BEGIN:
            if (s_sampling) {
                LogError << "calib: error manual-busy";
                break;
            }
            for (int ch = 0; ch < 4; ch++) {
                s_manualN[ch] = 0;
                s_manualFitted[ch] = false;
                s_lastRaw[ch] = 0;
            }
            s_manualOn = true;
            LogInfo << "calib: mbegin";
            LogInfo << "calib: board " << kBoardDir;
            snprintf(m, sizeof(m), "calib: mask 0x%X", (unsigned)s_rec.validMask);
            LogInfo << m;
            break;
        case OP_HOST_MANUAL_ABORT:
            for (int ch = 0; ch < 4; ch++) {
                s_manualN[ch] = 0;
                s_manualFitted[ch] = false;
            }
            s_sampling = false;
            s_samplingCh = -1;
            s_manualOn = false;
            LogInfo << "calib: mabort";
            break;
        case OP_HOST_MANUAL_SET_RAW:
            if (!s_manualOn) {
                LogError << "calib: error manual-off";
                break;
            }
            if (s_sampling) {
                LogError << "calib: error manual-busy";
                break;
            }
#if defined(CONFIG_DEVICE_DA_4CH)
            if (len >= 4) {
                uint8_t ch = p[1] & 0x03;
                uint16_t raw = (uint16_t)(p[2] | (p[3] << 8));
                Bd().CalibDaSet(ch, raw);
                s_lastRaw[ch] = raw;
                snprintf(m, sizeof(m), "calib: mset ch=%u raw=%u", (unsigned)ch, (unsigned)raw);
                LogInfo << m;
            } else {
                LogError << "calib: error bad-op";
            }
#else
            LogError << "calib: error bad-op";
#endif
            break;
        case OP_HOST_MANUAL_ADD:
            if (!s_manualOn) {
                LogError << "calib: error manual-off";
                break;
            }
            if (s_sampling) {
                LogError << "calib: error manual-busy";
                break;
            }
            if (len < 4) break;
            {
                uint8_t ch = p[1] & 0x03;
                uint16_t ideal = (uint16_t)(p[2] | (p[3] << 8));
                if (s_manualN[ch] >= kManualMax) {
                    LogError << "calib: error manual-full";
                    break;
                }
                if (kManualAd) {
                    s_sampling = true;
                    s_samplingCh = ch;
                    s_samplingIdeal = ideal;
                    s_mWaitMs = 0;
                    s_mStable = 0;
                    s_mMin = 0xFFFF;
                    s_mMax = 0;
                } else {
                    int n = s_manualN[ch];
                    s_mx[ch][n] = ideal;
                    s_my[ch][n] = s_lastRaw[ch];
                    s_manualN[ch] = n + 1;
                    s_manualFitted[ch] = false;
                    snprintf(m, sizeof(m), "calib: mpt ch=%u n=%u ideal=%u raw=%u", (unsigned)ch, (unsigned)(n + 1),
                             (unsigned)ideal, (unsigned)s_lastRaw[ch]);
                    LogInfo << m;
                }
            }
            break;
        case OP_HOST_MANUAL_FIT:
            if (!s_manualOn) {
                LogError << "calib: error manual-off";
                break;
            }
            if (s_sampling) {
                LogError << "calib: error manual-busy";
                break;
            }
            ManualFit();
            break;
        case OP_HOST_MANUAL_SAVE:
            if (!s_manualOn) {
                LogError << "calib: error manual-off";
                break;
            }
            if (s_sampling) {
                LogError << "calib: error manual-busy";
                break;
            }
            ManualSave();
            break;
        case OP_HOST_READ_AD:
            for (uint8_t ch = 0; ch < 4; ch++) {
#if defined(CONFIG_DEVICE_AD_4CH)
                uint16_t raw = Bd().CalibAdRaw(ch);
                float ideal = Bd().CalibAdIdeal(ch, raw);
                snprintf(m, sizeof(m), "calib: ad ch=%u raw=%u ideal=%.1f", (unsigned)ch, (unsigned)raw, (double)ideal);
#else
                snprintf(m, sizeof(m), "calib: ad ch=%u raw=%u", (unsigned)ch, (unsigned)s_lastRaw[ch]);
#endif
                LogInfo << m;
            }
            break;
        case OP_HOST_STATUS:
            PrintDevice();
            LogInfo << "calib: role MANUAL";
            LogInfo << "calib: board " << kBoardDir;
            snprintf(m, sizeof(m), "calib: mask 0x%X", (unsigned)s_rec.validMask);
            LogInfo << m;
            break;
        default:
            LogError << "calib: error bad-op";
            break;
    }
}

void HostPoll() {
    char tmp[64];
    int n = RttRead(tmp, sizeof(tmp));
    if (n > 0) {
        if (s_cmdLen + n > (int)sizeof(s_cmdRx)) {
            s_cmdLen = 0;
        } else {
            std::memcpy(s_cmdRx + s_cmdLen, tmp, n);
            s_cmdLen += n;
        }
    }

    int pos = 0;
    while (pos < s_cmdLen) {
        if (s_cmdRx[pos] != CALIB_FRAME_HEAD) {
            pos++;
            continue;
        }
        if (s_cmdLen - pos < 2) break;
        uint8_t plen = s_cmdRx[pos + 1];
        if (plen > CALIB_MAX_PAYLOAD) {
            pos++;
            continue;
        }
        int total = 2 + plen + 4;
        if (s_cmdLen - pos < total) break;

        const uint8_t* payload = s_cmdRx + pos + 2;
        uint32_t c = 0;
        uint8_t* cb = reinterpret_cast<uint8_t*>(&c);
        for (int i = 0; i < 4; i++) cb[i] = s_cmdRx[pos + 2 + plen + i];
        if (CalibCrc().Calculate(reinterpret_cast<const char*>(payload), plen) == c) {
            HandleManual(payload, plen);
            pos += total;
        } else {
            pos++;
        }
    }

    if (pos > 0) {
        int rem = s_cmdLen - pos;
        if (rem > 0) std::memmove(s_cmdRx, s_cmdRx + pos, rem);
        s_cmdLen = rem;
    }
}

// AD 采点：与 DoSweep 的 PH_SETTLE 同节奏——先等 kSettleMs，再每 kSampleIntervalMs 采 kStableCount 次，
// 取窗内 (min+max)/2
void ManualTick() {
    HostPoll();
#if defined(CONFIG_DEVICE_AD_4CH)
    if (!s_sampling) return;
    s_mWaitMs++;
    if (s_mStable == 0) {
        if (s_mWaitMs < kSettleMs) return;
        s_mWaitMs = 0;
        s_mStable = 1;
    } else if (s_mWaitMs < kSampleIntervalMs) {
        return;
    } else {
        s_mWaitMs = 0;
    }
    uint16_t v = Bd().CalibAdRaw((uint8_t)s_samplingCh);
    if (v < s_mMin) s_mMin = v;
    if (v > s_mMax) s_mMax = v;
    if (s_mStable < kStableCount) {
        s_mStable++;
        return;
    }
    uint16_t median = (uint16_t)((s_mMin + s_mMax) / 2);
    int ch = s_samplingCh;
    int n = s_manualN[ch];
    s_mx[ch][n] = median;
    s_my[ch][n] = s_samplingIdeal;
    s_manualN[ch] = n + 1;
    s_manualFitted[ch] = false;
    s_sampling = false;
    char m[80];
    snprintf(m, sizeof(m), "calib: mpt ch=%u n=%u ideal=%u raw=%u", (unsigned)ch, (unsigned)(n + 1),
             (unsigned)s_samplingIdeal, (unsigned)median);
    LogInfo << m;
#endif
}

#else

struct RefInfo {
    uint8_t board, major, minor, has, degree, chCount;
    uint16_t mask;
};

// 阻塞 ~50ms 等 REF 的 INFO 应答；仅非运行态调用，不与扫描争用 CalibLink
bool RefGetInfo(RefInfo* out) {
    static SysDelay delay;
    uint8_t p = OP_GET_INFO;
    CalibLinkSend(&p, 1);
    for (int i = 0; i < 50; i++) {
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) == 0 && len >= 9 && payload[0] == OP_ACK_INFO) {
            out->board = payload[1];
            out->major = payload[2];
            out->minor = payload[3];
            out->has = payload[4];
            out->degree = payload[5];
            out->chCount = payload[6];
            out->mask = (uint16_t)(payload[7] | (payload[8] << 8));
            return true;
        }
        delay.DelayMs(1);
    }
    return false;
}

// 最近一次 REF 应答：0 = 未应答。S_PING 成功后校验 REF 板型方向是否匹配
uint8_t s_refBoard = 0;
bool s_refAnswered = false;
#if defined(CONFIG_DEVICE_AD_4CH)
constexpr uint8_t kWantRefBoard = 2;  // AD 板作 DUT 时必须配 DA 板 REF
#else
constexpr uint8_t kWantRefBoard = 1;  // DA 板作 DUT/CHK 时必须配 AD 板 REF
#endif

void LogRefInfo(bool warn) {
    RefInfo info;
    char m[96];
    if (!RefGetInfo(&info)) {
        LogInfo << "calib: ref no-reply";
        s_refAnswered = false;
        s_refBoard = 0;
        return;
    }
    s_refAnswered = true;
    s_refBoard = info.board;
    snprintf(m, sizeof(m), "calib: ref board=%u ver=%u.%u has=%u mask=0x%X", (unsigned)info.board, (unsigned)info.major,
             (unsigned)info.minor, (unsigned)info.has, (unsigned)info.mask);
    LogInfo << m;
    if (warn) {
        for (int ch = 0; ch < 4; ch++) {
            if (!(s_chMask & (1u << ch))) continue;
            uint8_t da = s_refDa[ch];
            if (!(info.mask & (1u << da))) {
                snprintf(m, sizeof(m), "calib: ref warn ch=%u not calibrated", (unsigned)da);
                LogInfo << m;
            }
        }
    }
}

#if defined(CONFIG_CALIB_ROLE_DUT)

uint8_t s_cmdRx[64];
int s_cmdLen = 0;

// 阻塞 ~50ms 等 REF 的 SET_DA 应答；仅非运行态调用，不与扫描争用 CalibLink
bool RefSetDa(uint8_t ch, uint16_t ideal) {
    static SysDelay delay;
    uint8_t p[4];
    p[0] = OP_SET_DA;
    p[1] = ch;
    std::memcpy(p + 2, &ideal, 2);
    CalibLinkSend(p, 4);
    for (int i = 0; i < 50; i++) {
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) == 0 && len >= 1) {
            if (payload[0] == OP_ACK_SET_DA) return true;
            if (payload[0] == OP_NACK) return false;
        }
        delay.DelayMs(1);
    }
    return false;
}

// 阻塞 ~50ms 等 REF 的 READ_AD 应答；仅非运行态调用，不与扫描争用 CalibLink
bool RefReadAd(float* out4) {
    static SysDelay delay;
    uint8_t p[1] = {OP_READ_AD};
    CalibLinkSend(p, 1);
    for (int i = 0; i < 50; i++) {
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) == 0 && len >= 17 && payload[0] == OP_ACK_READ_AD) {
            for (int ch = 0; ch < 4; ch++) std::memcpy(out4 + ch, payload + 1 + 4 * ch, 4);
            return true;
        }
        delay.DelayMs(1);
    }
    return false;
}

void HandleHost(const uint8_t* p, uint8_t len) {
    char m[128];
    switch (p[0]) {
        case OP_HOST_CONFIG:
            if (len >= 6) {
                s_chMask = p[1] & 0x0F;
                for (int i = 0; i < 4; i++) s_refDa[i] = p[2 + i] & 0x03;
                if (s_chMask == 0) {
                    LogError << "calib: error empty-mask";
                } else {
                    snprintf(m, sizeof(m), "calib: cfg mask=0x%X da=%u,%u,%u,%u", (unsigned)s_chMask,
                             (unsigned)s_refDa[0], (unsigned)s_refDa[1], (unsigned)s_refDa[2], (unsigned)s_refDa[3]);
                    LogInfo << m;
                }
            }
            if (s_state == S_IDLE || s_state == S_DONE || s_state == S_FAIL || s_paused) LogRefInfo(true);
            break;
        case OP_HOST_START:
            if (s_state == S_IDLE || s_state == S_DONE || s_state == S_FAIL) {
                s_paused = false;
                ResetRunCounters();
                ConfigFromMask();
                if (s_runCount == 0) {
                    LogError << "calib: error empty-mask";
                } else {
                    s_state = S_PING;
                    LogInfo << "calib: reset";
                    PrintState();
                }
            } else {
                LogError << "calib: error busy";
            }
            break;
        case OP_HOST_PAUSE:
            if (s_state != S_IDLE && s_state != S_DONE && s_state != S_FAIL) {
                s_paused = true;
                PrintState();
            }
            break;
        case OP_HOST_RESUME:
            s_paused = false;
            PrintState();
            break;
        case OP_HOST_RESTART:
            s_paused = false;
            ResetRunCounters();
            ConfigFromMask();
            if (s_runCount == 0) {
                LogError << "calib: error empty-mask";
            } else {
                s_state = S_PING;
                LogInfo << "calib: reset";
                PrintState();
            }
            break;
        case OP_HOST_STOP:
            s_state = S_IDLE;
            s_paused = false;
            s_runCount = 0;
            s_sweepIdx = 0;
            PrintState();
            break;
        case OP_HOST_SET_DA:
            if (len >= 4) {
                if (s_state == S_IDLE || s_state == S_DONE || s_state == S_FAIL || s_paused) {
                    uint8_t ch = p[1] & 0x03;
                    uint16_t v = (uint16_t)(p[2] | (p[3] << 8));
#if defined(CONFIG_DEVICE_AD_4CH)
                    if (RefSetDa(ch, v)) {
                        snprintf(m, sizeof(m), "calib: set_da ch=%u ideal=%u", (unsigned)ch, (unsigned)v);
                        LogInfo << m;
                    } else {
                        LogError << "calib: error set_da timeout";
                    }
#else
                    Bd().CalibDaSet(ch, v);
                    snprintf(m, sizeof(m), "calib: set_da ch=%u raw=%u", (unsigned)ch, (unsigned)v);
                    LogInfo << m;
#endif
                } else {
                    LogError << "calib: error busy";
                }
            }
            break;
        case OP_HOST_READ_AD:
#if defined(CONFIG_DEVICE_AD_4CH)
            for (uint8_t ch = 0; ch < 4; ch++) {
                uint16_t raw = Bd().CalibAdRaw(ch);
                float ideal = Bd().CalibAdIdeal(ch, raw);
                snprintf(m, sizeof(m), "calib: ad ch=%u raw=%u ideal=%.1f", (unsigned)ch, (unsigned)raw, (double)ideal);
                LogInfo << m;
            }
#else
            if (s_state != S_IDLE && s_state != S_DONE && s_state != S_FAIL && !s_paused) {
                LogError << "calib: error busy";
            } else {
                float v[4];
                if (!RefReadAd(v)) {
                    LogError << "calib: error read_ad timeout";
                } else {
                    for (uint8_t ch = 0; ch < 4; ch++) {
                        snprintf(m, sizeof(m), "calib: refad ch=%u ideal=%.1f", (unsigned)ch, (double)v[ch]);
                        LogInfo << m;
                    }
                }
            }
#endif
            break;
        case OP_HOST_STATUS:
            LogInfo << "calib: role DUT";
            PrintState();
            snprintf(m, sizeof(m), "calib: cfg mask=0x%X da=%u,%u,%u,%u", (unsigned)s_chMask, (unsigned)s_refDa[0],
                     (unsigned)s_refDa[1], (unsigned)s_refDa[2], (unsigned)s_refDa[3]);
            LogInfo << m;
            snprintf(m, sizeof(m), "calib: mask 0x%X", (unsigned)s_rec.validMask);
            LogInfo << m;
            for (uint8_t ch = 0; ch < 4; ch++) {
                if (!(s_rec.validMask & (1u << ch))) continue;
                char cm[128];
                int cn = snprintf(cm, sizeof(cm), "calib: coeff ch%u", (unsigned)ch);
                for (int k = 0; k < POLY_COEFF_NUM; k++) {
                    cn += snprintf(cm + cn, sizeof(cm) - cn, " %.17g", s_rec.coeff[ch][k]);
                }
                LogInfo << cm;
            }
            if (s_state == S_IDLE || s_state == S_DONE || s_state == S_FAIL || s_paused) LogRefInfo(true);
            break;
        default:
            LogError << "calib: error bad-op";
            break;
    }
}

void HostPoll() {
    char tmp[64];
    int n = RttRead(tmp, sizeof(tmp));
    if (n > 0) {
        if (s_cmdLen + n > (int)sizeof(s_cmdRx)) {
            s_cmdLen = 0;
        } else {
            std::memcpy(s_cmdRx + s_cmdLen, tmp, n);
            s_cmdLen += n;
        }
    }

    int pos = 0;
    while (pos < s_cmdLen) {
        if (s_cmdRx[pos] != CALIB_FRAME_HEAD) {
            pos++;
            continue;
        }
        if (s_cmdLen - pos < 2) break;
        uint8_t plen = s_cmdRx[pos + 1];
        if (plen > CALIB_MAX_PAYLOAD) {
            pos++;
            continue;
        }
        int total = 2 + plen + 4;
        if (s_cmdLen - pos < total) break;

        const uint8_t* payload = s_cmdRx + pos + 2;
        uint32_t c = 0;
        uint8_t* cb = reinterpret_cast<uint8_t*>(&c);
        for (int i = 0; i < 4; i++) cb[i] = s_cmdRx[pos + 2 + plen + i];
        if (CalibCrc().Calculate(reinterpret_cast<const char*>(payload), plen) == c) {
            HandleHost(payload, plen);
            pos += total;
        } else {
            pos++;
        }
    }

    if (pos > 0) {
        int rem = s_cmdLen - pos;
        if (rem > 0) std::memmove(s_cmdRx, s_cmdRx + pos, rem);
        s_cmdLen = rem;
    }
}

#endif  // CONFIG_CALIB_ROLE_DUT

void StartPoint() {
    s_retry = 0;
    s_waitMs = 0;
    s_stableCount = 0;
    for (int ch = 0; ch < 4; ch++) {
        s_stableMin[ch] = 0xFFFF;
        s_stableMax[ch] = 0;
    }
    if (s_sweepIdx == 0) {
        for (int r = 0; r < s_runCount; r++) {
            char rm[48];
            snprintf(rm, sizeof(rm), "calib: run ch=%u da=%u", (unsigned)s_runs[r].adCh, (unsigned)s_runs[r].daCh);
            LogInfo << rm;
        }
    }
#if defined(CONFIG_DEVICE_AD_4CH)
    // 并行：本点同时驱动全部去重后的 REF DA 源，随后一次读本机四路 AD
    s_driveCount = 0;
    for (int r = 0; r < s_runCount; r++) {
        uint8_t da = s_runs[r].daCh;
        bool dup = false;
        for (int d = 0; d < s_driveCount; d++) {
            if (s_driveCh[d] == da) dup = true;
        }
        if (!dup) s_driveCh[s_driveCount++] = da;
    }
    uint16_t g = kGridLive[s_sweepIdx];
    for (int d = 0; d < s_driveCount; d++) {
        uint8_t p[4];
        p[0] = OP_SET_DA;
        p[1] = s_driveCh[d];
        std::memcpy(p + 2, &g, 2);
        CalibLinkSend(p, 4);
    }
    s_ackCount = 0;
    s_phase = PH_WAIT;
#else
    for (int ch = 0; ch < 4; ch++) {
        uint16_t raw = kGridLive[s_sweepIdx];
        // CHK 比较的是「设定 ideal」与「量得 ideal」，驱动值先经本机曲线反算成 raw
        if (kCheckMode) raw = Bd().CalibDaRaw((uint8_t)ch, raw);
        Bd().CalibDaSet((uint8_t)ch, raw);
    }
    s_awaitReply = false;
    s_stableCount = 0;
    s_waitMs = 0;
    s_phase = PH_SETTLE;
#endif
}

void ChkPoint() {
    double d[4];
    const double set = (double)kGridLive[s_sweepIdx];
    for (int ch = 0; ch < 4; ch++) {
        d[ch] = (double)s_samples[ch][s_sweepIdx] - set;
        const double ad = d[ch] < 0 ? -d[ch] : d[ch];
        if (ad > s_chkMax[ch]) {
            s_chkMax[ch] = ad;
            s_chkMaxAt[ch] = kGridLive[s_sweepIdx];
        }
    }
    char msg[80];
    snprintf(msg, sizeof(msg), "calib: set=%u d=%+.1f,%+.1f,%+.1f,%+.1f", (unsigned)kGridLive[s_sweepIdx], d[0], d[1], d[2],
             d[3]);
    LogInfo << msg;
}

void AdvancePoint() {
    s_sweepIdx++;
    if (s_sweepIdx >= kGridN) {
        s_state = kCheckMode ? S_CHECK : S_FIT;
        return;
    }
    s_phase = PH_SEND;
}

void DoSweep() {
    if (s_sweepIdx >= kGridN) {
        s_state = kCheckMode ? S_CHECK : S_FIT;
        return;
    }

    if (s_phase == PH_SEND) {
        StartPoint();
        return;
    }

    if (s_phase == PH_WAIT) {
#if defined(CONFIG_DEVICE_AD_4CH)
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) == 0) {
            if (len >= 1 && payload[0] == OP_ACK_SET_DA) {
                s_ackCount++;
                if (s_ackCount >= s_driveCount) {
                    s_waitMs = 0;
                    s_phase = PH_SETTLE;
                }
            }
            return;
        }
        s_waitMs++;
        if (s_waitMs < kReplyTimeoutMs) return;
        s_retry++;
        if (s_retry >= kRetryMax) {
            LogError << "calib: ref timeout";
            EnterFail();
            return;
        }
        s_waitMs = 0;
        s_phase = PH_SEND;
        return;
#else
        // DA 方向：交替「发 OP_READ_AD → 收 ACK」攒窗，每点 kStableCount 次往返
        if (!s_awaitReply) {
            // 采样间留 kRefSampleIntervalMs，使窗长(16×20ms=320ms)跨过 REF 的 128ms 均值滤波
            if (s_stableCount > 0 && s_waitMs < kRefSampleIntervalMs) {
                s_waitMs++;
                return;
            }
            s_waitMs = 0;
            uint8_t p[1] = {OP_READ_AD};
            CalibLinkSend(p, 1);
            s_awaitReply = true;
            return;
        }
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) != 0) {
            s_waitMs++;
            if (s_waitMs < kReplyTimeoutMs) return;
            s_retry++;
            if (s_retry >= kRetryMax) {
                LogError << "calib: ref timeout";
                EnterFail();
                return;
            }
            s_waitMs = 0;
            s_awaitReply = false;  // 重发同一请求
            return;
        }
        if (len < 17 || payload[0] != OP_ACK_READ_AD) return;  // 忽略其它帧，等超时
        s_awaitReply = false;
        for (int ch = 0; ch < 4; ch++) {
            float v;
            std::memcpy(&v, payload + 1 + 4 * ch, 4);
            s_measWin[ch][s_stableCount] = v;
            if (s_stableCount == 0) {
                s_measMin[ch] = v;
                s_measMax[ch] = v;
            } else {
                if (v < s_measMin[ch]) s_measMin[ch] = v;
                if (v > s_measMax[ch]) s_measMax[ch] = v;
            }
        }
        s_stableCount++;
        if (s_stableCount < kStableCount) return;
        for (int ch = 0; ch < 4; ch++) {
            s_measMid[ch] = MedianWin(s_measWin[ch], kStableCount);
        }
        if (kCheckMode) {
            for (int ch = 0; ch < 4; ch++) {
                s_samples[ch][s_sweepIdx] = s_measMid[ch];
            }
            char raw[96];
            snprintf(raw, sizeof(raw), "calib: raw m=%.1f,%.1f,%.1f,%.1f", (double)s_samples[0][s_sweepIdx],
                     (double)s_samples[1][s_sweepIdx], (double)s_samples[2][s_sweepIdx],
                     (double)s_samples[3][s_sweepIdx]);
            LogInfo << raw;
            ChkPoint();
        } else {
            for (int r = 0; r < s_runCount; r++) {
                uint8_t ch = s_runs[r].adCh;
                int src = s_runs[r].daCh;
                float mid = s_measMid[src];
                s_samples[ch][s_sweepIdx] = mid;
                bool settled = s_measMax[src] - s_measMin[src] <= (float)kStableCode;
                char m[112];
                snprintf(m, sizeof(m), "calib: dpt%u ch=%u ideal=%u meas=%u%s", (unsigned)s_sweepIdx, (unsigned)ch,
                         (unsigned)kGridLive[s_sweepIdx], (unsigned)mid, settled ? "" : " unstable");
                LogInfo << m;
            }
        }
#if defined(CALIB_LOG_SPAN)
        if (!kCheckMode) {
            char sp[128];
            snprintf(sp, sizeof(sp), "calib: span%u min=%u,%u,%u,%u max=%u,%u,%u,%u", (unsigned)s_sweepIdx,
                     (unsigned)s_measMin[0], (unsigned)s_measMin[1], (unsigned)s_measMin[2], (unsigned)s_measMin[3],
                     (unsigned)s_measMax[0], (unsigned)s_measMax[1], (unsigned)s_measMax[2], (unsigned)s_measMax[3]);
            LogInfo << sp;
        }
#endif
        AdvancePoint();
        return;
#endif
    }

    if (s_phase == PH_SETTLE) {
        s_waitMs++;
#if defined(CONFIG_DEVICE_AD_4CH)
        // 先固定等 kSettleMs 让 DA 建压，之后每 kSampleIntervalMs 采一次，攒够 kStableCount 次
        if (s_stableCount == 0) {
            if (s_waitMs < kSettleMs) return;
            s_waitMs = 0;
            s_stableCount = 1;
        } else if (s_waitMs < kSampleIntervalMs) {
            return;
        } else {
            s_waitMs = 0;
        }
        for (int ch = 0; ch < 4; ch++) {
            uint16_t v = Bd().CalibAdRaw((uint8_t)ch);
            if (v < s_stableMin[ch]) s_stableMin[ch] = v;
            if (v > s_stableMax[ch]) s_stableMax[ch] = v;
        }
        if (s_stableCount < kStableCount) {
            s_stableCount++;
            return;
        }
        bool settled = true;
        for (int ch = 0; ch < 4; ch++) {
            if (s_stableMax[ch] - s_stableMin[ch] > kStableCode) settled = false;
        }
        for (int r = 0; r < s_runCount; r++) {
            uint8_t ch = s_runs[r].adCh;
            float mid = 0.5f * ((float)s_stableMin[ch] + (float)s_stableMax[ch]);
            s_samples[ch][s_sweepIdx] = mid;
            char m[112];
            snprintf(m, sizeof(m), "calib: dpt%u ch=%u ideal=%u meas=%u%s", (unsigned)s_sweepIdx, (unsigned)ch,
                     (unsigned)kGridLive[s_sweepIdx], (unsigned)mid, settled ? "" : " unstable");
            LogInfo << m;
        }
#if defined(CALIB_LOG_SPAN)
        {
            char sp[128];
            snprintf(sp, sizeof(sp), "calib: span%u min=%u,%u,%u,%u max=%u,%u,%u,%u", (unsigned)s_sweepIdx,
                     (unsigned)s_stableMin[0], (unsigned)s_stableMin[1], (unsigned)s_stableMin[2],
                     (unsigned)s_stableMin[3], (unsigned)s_stableMax[0], (unsigned)s_stableMax[1],
                     (unsigned)s_stableMax[2], (unsigned)s_stableMax[3]);
            LogInfo << sp;
        }
#endif
        AdvancePoint();
#else
        // DA 方向：等 DA 建压后进入「多次读 REF AD」回环
        if (s_waitMs < kSettleMs) return;
        s_waitMs = 0;
        s_stableCount = 0;
        s_awaitReply = false;
        s_phase = PH_WAIT;
#endif
        return;
    }
}

#if !defined(CONFIG_CALIB_ROLE_CHK)

void AdvVerifyPoint() {
    s_verifyIdx++;
    s_stableCount = -1;
    s_phase = PH_SEND;
}

void ChkVerifyPoint(uint8_t ch) {
    const double set = (double)kGridLive[s_verifyIdx];
    double d = PolyEval(s_rec.coeff[ch], (double)s_verify[ch][s_verifyIdx]) - set;
    s_verSumSq[ch] += d * d;
    double ad = d < 0 ? -d : d;
    if (ad > s_verMax[ch]) {
        s_verMax[ch] = ad;
        s_verMaxAt[ch] = kGridLive[s_verifyIdx];
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "calib: vfy set=%u ch%u d=%+.1f", (unsigned)kGridLive[s_verifyIdx], (unsigned)ch, d);
    LogInfo << msg;
}

void DoVerifySweep() {
    if (s_verifyIdx >= kGridN) {
        s_state = S_VERIFY;
        return;
    }

    if (s_phase == PH_SEND) {
        if (s_stableCount < 0) {
            s_retry = 0;
            s_waitMs = 0;
            s_stableCount = 0;
            for (int ch = 0; ch < 4; ch++) {
                s_stableMin[ch] = 0xFFFF;
                s_stableMax[ch] = 0;
            }
        }
#if defined(CONFIG_DEVICE_AD_4CH)
        s_driveCount = 0;
        for (int r = 0; r < s_runCount; r++) {
            uint8_t da = s_runs[r].daCh;
            bool dup = false;
            for (int d = 0; d < s_driveCount; d++) {
                if (s_driveCh[d] == da) dup = true;
            }
            if (!dup) s_driveCh[s_driveCount++] = da;
        }
        uint16_t ideal = kGridLive[s_verifyIdx];
        for (int d = 0; d < s_driveCount; d++) {
            uint8_t p[4];
            p[0] = OP_SET_DA;
            p[1] = s_driveCh[d];
            std::memcpy(p + 2, &ideal, 2);
            CalibLinkSend(p, 4);
        }
        s_ackCount = 0;
        s_phase = PH_WAIT;
#else
        // DA 方向：本机写 DA，等 REF 回读 AD
        for (int ch = 0; ch < 4; ch++) {
            Bd().CalibDaSet((uint8_t)ch, kGridLive[s_verifyIdx]);
        }
        s_waitMs = 0;
        s_stableCount = 0;
        s_awaitReply = false;
        s_phase = PH_SETTLE;
#endif
        return;
    }

    if (s_phase == PH_WAIT) {
#if defined(CONFIG_DEVICE_AD_4CH)
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) == 0) {
            if (len >= 1 && payload[0] == OP_ACK_SET_DA) {
                s_ackCount++;
                if (s_ackCount >= s_driveCount) {
                    s_waitMs = 0;
                    s_phase = PH_SETTLE;
                }
            }
            return;
        }
        s_waitMs++;
        if (s_waitMs < kReplyTimeoutMs) return;
        s_retry++;
        if (s_retry >= kRetryMax) {
            LogError << "calib: verify ref timeout";
            EnterFail();
            return;
        }
        s_waitMs = 0;
        s_phase = PH_SEND;
        return;
#else
        // DA 方向：交替「发 OP_READ_AD → 收 ACK」攒窗
        if (!s_awaitReply) {
            // 同 DoSweep：采样间留 kRefSampleIntervalMs，让窗跨过 REF 的 128ms 均值滤波
            if (s_stableCount > 0 && s_waitMs < kRefSampleIntervalMs) {
                s_waitMs++;
                return;
            }
            s_waitMs = 0;
            uint8_t p[1] = {OP_READ_AD};
            CalibLinkSend(p, 1);
            s_awaitReply = true;
            return;
        }
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) != 0) {
            s_waitMs++;
            if (s_waitMs < kReplyTimeoutMs) return;
            s_retry++;
            if (s_retry >= kRetryMax) {
                LogError << "calib: verify ref timeout";
                EnterFail();
                return;
            }
            s_waitMs = 0;
            s_awaitReply = false;
            return;
        }
        if (len < 17 || payload[0] != OP_ACK_READ_AD) return;
        s_awaitReply = false;
        for (int ch = 0; ch < 4; ch++) {
            float v;
            std::memcpy(&v, payload + 1 + 4 * ch, 4);
            s_measWin[ch][s_stableCount] = v;
            if (s_stableCount == 0) {
                s_measMin[ch] = v;
                s_measMax[ch] = v;
            } else {
                if (v < s_measMin[ch]) s_measMin[ch] = v;
                if (v > s_measMax[ch]) s_measMax[ch] = v;
            }
        }
        s_stableCount++;
        if (s_stableCount < kStableCount) return;
        for (int ch = 0; ch < 4; ch++) {
            s_measMid[ch] = MedianWin(s_measWin[ch], kStableCount);
        }
        for (int r = 0; r < s_runCount; r++) {
            uint8_t ch = s_runs[r].adCh;
            int src = s_runs[r].daCh;
            s_verify[ch][s_verifyIdx] = s_measMid[src];
            bool settled = s_measMax[src] - s_measMin[src] <= (float)kStableCode;
            char m[112];
            snprintf(m, sizeof(m), "calib: dvfy%u ch=%u set=%u meas=%u%s", (unsigned)s_verifyIdx, (unsigned)ch,
                     (unsigned)kGridLive[s_verifyIdx], (unsigned)s_verify[ch][s_verifyIdx], settled ? "" : " unstable");
            LogInfo << m;
            ChkVerifyPoint(ch);
        }
        AdvVerifyPoint();
        return;
#endif
    }

#if defined(CONFIG_DEVICE_AD_4CH)
    // PH_SETTLE：等 REF 建压后本地多次采样，取窗内 min/max 中值
    s_waitMs++;
    if (s_stableCount == 0) {
        if (s_waitMs < kSettleMs) return;
        s_waitMs = 0;
        s_stableCount = 1;
    } else if (s_waitMs < kSampleIntervalMs) {
        return;
    } else {
        s_waitMs = 0;
    }
    for (int ch = 0; ch < 4; ch++) {
        uint16_t v = Bd().CalibAdRaw((uint8_t)ch);
        if (v < s_stableMin[ch]) s_stableMin[ch] = v;
        if (v > s_stableMax[ch]) s_stableMax[ch] = v;
    }
    if (s_stableCount < kStableCount) {
        s_stableCount++;
        return;
    }
    for (int r = 0; r < s_runCount; r++) {
        uint8_t ch = s_runs[r].adCh;
        bool settled = s_stableMax[ch] - s_stableMin[ch] <= kStableCode;
        s_verify[ch][s_verifyIdx] = 0.5f * ((float)s_stableMin[ch] + (float)s_stableMax[ch]);
        char m[112];
        snprintf(m, sizeof(m), "calib: dvfy%u ch=%u set=%u meas=%u%s", (unsigned)s_verifyIdx, (unsigned)ch,
                 (unsigned)kGridLive[s_verifyIdx], (unsigned)s_verify[ch][s_verifyIdx], settled ? "" : " unstable");
        LogInfo << m;
        ChkVerifyPoint(ch);
    }
    AdvVerifyPoint();
#else
    // DA 方向：等本机 DA 建压 → 读 REF AD 攒窗
    s_waitMs++;
    if (s_waitMs < kSettleMs) return;
    s_waitMs = 0;
    s_stableCount = 0;
    s_awaitReply = false;
    s_phase = PH_WAIT;
#endif
}

void DoVerify() {
    if (!s_verifyReported) {
        for (int ch = 0; ch < 4; ch++) {
            s_verMax[ch] = 0.0;
            for (int i = 0; i < kGridN; i++) s_verSumSq[ch] = 0.0;
        }
        LogInfo << "calib: verify start";
        s_phase = PH_SEND;
        s_verifyReported = true;
        return;
    }

    char msg[256];
    int n = snprintf(msg, sizeof(msg), "calib: verify");
    bool pass = true;
    bool first = true;
    for (int ch = 0; ch < 4; ch++) {
        if (!(s_chMask & (1u << ch))) continue;
        double rmse = sqrt(s_verSumSq[ch] / kGridN);
        n += snprintf(msg + n, sizeof(msg) - n, "%s ch%d max=%.1f @set=%u rmse=%.1f", first ? "" : ";", ch, s_verMax[ch],
                      (unsigned)s_verMaxAt[ch], rmse);
        first = false;
        if (s_verMax[ch] >= kCheckGateCode) pass = false;
    }
    LogInfo << msg;

    bool anyBad = false;
    for (int ch = 0; ch < 4; ch++) {
        if (!(s_chMask & (1u << ch))) continue;
        if (s_verMax[ch] < kCheckGateCode) continue;
        char ol[256];
        int m = snprintf(ol, sizeof(ol), "calib: vfy ch%d |d|>=%d at:", ch, (int)kCheckGateCode);
        for (int i = 0; i < kGridN && m < (int)sizeof(ol) - 16; i++) {
            double d = PolyEval(s_rec.coeff[ch], (double)s_verify[ch][i]) - (double)kGridLive[i];
            if (d < 0) d = -d;
            if (d >= kCheckGateCode) m += snprintf(ol + m, sizeof(ol) - m, " %u(%.0f)", (unsigned)kGridLive[i], d);
        }
        LogInfo << ol;
        anyBad = true;
    }
    char res[64];
    snprintf(res, sizeof(res), "calib: verify %s", pass ? "PASS" : "FAIL");
    if (anyBad) {
        LogError << res;
    } else {
        LogInfo << res;
    }
    s_state = S_DONE;
    LedSet(true);
}

#endif  // !CONFIG_CALIB_ROLE_CHK

// 导出采样点与系数供 PC 端复算；每 tick 只发一行，且行间留出间隔让 RTT 主机读走
void DoDump() {
    enum { kChunkVals = 24, kStr = 200, kLineGapMs = 100 };
    if (s_dumpWait > 0) {
        s_dumpWait--;
        return;
    }
    const int perCh = (kGridN + kChunkVals - 1) / kChunkVals;
    const int gridLines = perCh;
    const int chLines = perCh * s_runCount;
    char dl[kStr];

    if (s_dumpLine < gridLines) {
        int off = s_dumpLine * kChunkVals;
        int n = snprintf(dl, sizeof(dl), "calib: dump grid %d", off);
        for (int i = off; i < kGridN && i < off + kChunkVals; i++) {
            n += snprintf(dl + n, sizeof(dl) - n, " %u", (unsigned)kGridLive[i]);
        }
        LogInfo << dl;
        s_dumpLine++;
        s_dumpWait = kLineGapMs;
        return;
    }

    if (s_dumpLine < gridLines + chLines) {
        int j = s_dumpLine - gridLines;
        int run = j / perCh;
        int off = (j % perCh) * kChunkVals;
        int ch = s_runs[run].adCh;
        int n = snprintf(dl, sizeof(dl), "calib: dump ch%d %d", ch, off);
        for (int i = off; i < kGridN && i < off + kChunkVals; i++) {
            n += snprintf(dl + n, sizeof(dl) - n, " %u", (unsigned)s_samples[ch][i]);
        }
        LogInfo << dl;
        s_dumpLine++;
        s_dumpWait = kLineGapMs;
        return;
    }

    if (s_dumpLine < gridLines + chLines + s_runCount) {
        int run = s_dumpLine - gridLines - chLines;
        int ch = s_runs[run].adCh;
        int n = snprintf(dl, sizeof(dl), "calib: coeff ch%d", ch);
        for (int k = 0; k < POLY_COEFF_NUM; k++) {
            n += snprintf(dl + n, sizeof(dl) - n, " %.17g", s_coeff[ch][k]);
        }
        LogInfo << dl;
        s_dumpLine++;
        s_dumpWait = kLineGapMs;
        return;
    }

    s_state = S_SAVE;
}

void FitAll() {
    for (int r = 0; r < s_runCount; r++) {
        const uint8_t ch = s_runs[r].adCh;
        double xs[kGridN];
        for (int i = 0; i < kGridN; i++) {
            xs[i] = (double)s_samples[ch][i];
        }
        if (!PolyFit(xs, s_gridD, kGridN, s_coeff[ch])) {
            LogError << "calib: fit failed ch=" << (unsigned)ch;
            EnterFail();
            return;
        }
        double maxRes = 0.0;
        double sumSq = 0.0;
        int maxAt = 0;
        for (int i = 0; i < kGridN; i++) {
            double d = PolyEval(s_coeff[ch], xs[i]) - s_gridD[i];
            sumSq += d * d;
            if (d < 0) d = -d;
            if (d > maxRes) {
                maxRes = d;
                maxAt = i;
            }
        }
        char st[128];
        snprintf(st, sizeof(st), "calib: ch%d resid max=%.1f @ideal=%u rmse=%.1f", ch, maxRes,
                 (unsigned)kGridLive[maxAt], sqrt(sumSq / kGridN));
        LogInfo << st;
        if (maxRes > kResidualLimitCode) {
            char rs[256];
            int n = snprintf(rs, sizeof(rs), "calib: ch%d resid>%d at:", ch, (int)kResidualLimitCode);
            for (int i = 0; i < kGridN && n < (int)sizeof(rs) - 16; i++) {
                double d = PolyEval(s_coeff[ch], xs[i]) - s_gridD[i];
                if (d < 0) d = -d;
                if (d > kResidualLimitCode) {
                    n += snprintf(rs + n, sizeof(rs) - n, " %u(%.0f)", (unsigned)kGridLive[i], d);
                }
            }
            LogError << rs;
            EnterFail();
            return;
        }

        // anchor dev：诊断该 ch 拟合曲线与本机内建曲线的偏差
        double dev = 0.0;
        for (int i = 0; i < kGridN; i++) {
#if defined(CONFIG_DEVICE_AD_4CH)
            double builtin = Bd().CalibBuiltinIdealFromRaw((uint16_t)xs[i]);
#else
            double builtin = Bd().CalibBuiltinRawFromIdeal((uint16_t)xs[i]);
#endif
            double d = PolyEval(s_coeff[ch], xs[i]) - builtin;
            if (d < 0) d = -d;
            if (d > dev) dev = d;
        }
        char devMsg[48];
        snprintf(devMsg, sizeof(devMsg), "calib: ch%u anchor dev = %g", (unsigned)ch, dev);
        LogInfo << devMsg;
    }

    s_dumpLine = 0;
    s_dumpWait = 0;
    s_state = S_DUMP;
}

void DoSave() {
    CalibRecord rec;
    if (!CalibStoreLoad(&rec)) {
        rec = CalibRecord{};
        rec.validMask = 0;
    }
#if defined(CONFIG_DEVICE_AD_4CH)
    rec.board = 1;
#else
    rec.board = 2;
#endif
    rec.degree = POLY_DEGREE;
    rec.channelCount = 4;
    for (int i = 0; i < s_runCount; i++) {
        uint8_t ch = s_runs[i].adCh;
        for (int k = 0; k < POLY_COEFF_NUM; k++) {
            rec.coeff[ch][k] = s_coeff[ch][k];
        }
        rec.validMask |= (uint16_t)(1u << ch);
    }
    if (!CalibStoreSave(&rec)) {
        LogError << "calib: flash write failed";
        EnterFail();
        return;
    }
    s_rec = rec;
    LogInfo << "calib: saved";
    {
        char m[48];
        snprintf(m, sizeof(m), "calib: mask 0x%X", (unsigned)s_rec.validMask);
        LogInfo << m;
    }
    s_verifyIdx = 0;
    s_stableCount = -1;
    s_verifyReported = false;
    s_phase = PH_SEND;
    s_state = S_VERIFY;
}

#if defined(CONFIG_CALIB_ROLE_CHK)
void DoCheck() {
    char msg[128];
    snprintf(msg, sizeof(msg), "calib: chk max |measured-set| = %.1f,%.1f,%.1f,%.1f", s_chkMax[0], s_chkMax[1],
             s_chkMax[2], s_chkMax[3]);
    LogInfo << msg;
    for (int ch = 0; ch < 4; ch++) {
        if (s_chkMax[ch] >= kCheckGateCode) {
            snprintf(msg, sizeof(msg), "calib: chk fail ch=%d at set=%u", ch, (unsigned)s_chkMaxAt[ch]);
            LogError << msg;
            EnterFail();
            return;
        }
    }
    LogInfo << "calib: chk pass";
    s_state = S_DONE;
    LedSet(true);
}
#endif

void DutTick() {
#if defined(CONFIG_CALIB_ROLE_DUT)
    HostPoll();
#endif
    if (s_paused) return;
    switch (s_state) {
        case S_IDLE:
            break;
        case S_PING: {
            uint8_t payload[40];
            uint8_t len;
            if (CalibLinkRecv(payload, &len) == 0 && len >= 1 && payload[0] == OP_PONG) {
                if (kCheckMode) {
                    LogInfo << "calib: role CHK";
                } else {
                    LogInfo << "calib: role DUT";
                }
                ConfigFromMask();
                LogRefInfo(true);
                if (!s_refAnswered) {
                    LogError << "calib: error ref no-reply";
                    EnterFail();
                    break;
                }
                if (s_refBoard != kWantRefBoard) {
                    LogError << "calib: error ref mismatch";
                    EnterFail();
                    break;
                }
                s_sweepIdx = 0;
                s_phase = PH_SEND;
                s_state = S_SWEEP;
                break;
            }
            if (s_pingCount >= kPingMax) {
                LogError << "calib: ref not found";
                EnterFail();
                break;
            }
            if (s_pingElapsed >= kPingIntervalMs) {
                uint8_t p = OP_PING;
                CalibLinkSend(&p, 1);
                s_pingCount++;
                s_pingElapsed = 0;
            }
            s_pingElapsed++;
            break;
        }
        case S_SWEEP:
            DoSweep();
            break;
        case S_FIT:
            FitAll();
            break;
        case S_DUMP:
            DoDump();
            break;
#if defined(CONFIG_CALIB_ROLE_CHK)
        case S_CHECK:
            DoCheck();
            break;
#endif
        case S_SAVE:
            DoSave();
            break;
#if !defined(CONFIG_CALIB_ROLE_CHK)
        case S_VERIFY:
            if (!s_verifyReported) {
                DoVerify();
            } else if (s_verifyIdx >= kGridN) {
                DoVerify();
            } else {
                DoVerifySweep();
            }
            break;
#endif
        case S_DONE:
            LedSet(true);
            break;
        case S_FAIL:
            s_failMs++;
            if (s_failMs >= 500) {
                s_failMs = 0;
                LedToggle();
            }
            break;
    }
}

#endif

}  // namespace

void CalibProcInit() {
#if !defined(CONFIG_CALIB_ROLE_MANUAL)
    CalibLinkInit();
#endif
#if defined(CALIB_FIXED_IDEAL)
    for (int i = 0; i < kGridN; i++) {
        kGridLive[i] = (uint16_t)(CALIB_FIXED_IDEAL);
    }
#endif
    for (int i = 0; i < kGridN; i++) {
        s_gridD[i] = (double)kGridLive[i];
    }

#if defined(CONFIG_CALIB_ROLE_MANUAL)
    PrintDevice();
    LogInfo << "calib: role MANUAL";
    LogInfo << "calib: board " << kBoardDir;
    CalibStoreLoad(&s_rec);
    {
        char m[48];
        snprintf(m, sizeof(m), "calib: mask 0x%X", (unsigned)s_rec.validMask);
        LogInfo << m;
    }
#else
#if defined(CONFIG_CALIB_ROLE_REF)
    LogInfo << "calib: role REF ready";
    LedSet(true);
#else
    if (kCheckMode) {
        s_state = S_PING;
        s_pingCount = 0;
        s_pingElapsed = kPingIntervalMs;
    } else {
#if defined(CALIB_AUTO_START)
        s_state = S_PING;
        s_pingCount = 0;
        s_pingElapsed = kPingIntervalMs;
#else
        s_state = S_IDLE;
#endif
    }
#endif

    bool loaded = CalibStoreLoad(&s_rec);
    if (loaded) {
        LogInfo << "calib: loaded";
    }
    PrintDevice();
    LogInfo << "calib: board " << kBoardDir;
#if defined(CONFIG_VERIFY_ON_BOOT)
    if (loaded) {
        ConfigFromMask();
        s_verifyIdx = 0;
        s_stableCount = -1;
        s_verifyReported = false;
        s_state = S_VERIFY;
    }
#endif
#endif
}

void CalibProcTick() {
#if defined(CONFIG_CALIB_ROLE_REF)
    RefTick();
#elif defined(CONFIG_CALIB_ROLE_MANUAL)
    ManualTick();
#else
    DutTick();
#endif
}
