#include "calib/CalibProc.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Application.h"
#include "CuteLogger/CuteLogger.h"
#include "at32f425_gpio.h"
#include "at32f425_wk_config.h"
#include "calib/CalibLink.h"
#include "calib/CalibStore.h"
#include "calib/PolyFit.h"

#if defined(CONFIG_CALIB_ROLE_CHK) && !defined(CONFIG_DEVICE_DA_4CH)
#error "CALIB_ROLE=CHK drives and measures DA, only valid for DA_4CH"
#endif

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

constexpr uint8_t OP_PING = 0x7F;
constexpr uint8_t OP_PONG = 0xFF;
constexpr uint8_t OP_SET_DA = 0x01;
constexpr uint8_t OP_ACK_SET_DA = 0x81;
constexpr uint8_t OP_READ_AD = 0x02;
constexpr uint8_t OP_ACK_READ_AD = 0x82;
constexpr uint8_t OP_NACK = 0x00;

enum State { S_PING, S_SWEEP, S_FIT, S_CHECK, S_DUMP, S_SAVE, S_VERIFY, S_DONE, S_FAIL };
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

State s_state = S_PING;
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
        default:
            resp[0] = OP_NACK;
            CalibLinkSend(resp, 1);
            break;
    }
}

#else

void StartPoint() {
    s_retry = 0;
    s_waitMs = 0;
    s_stableCount = 0;
    for (int ch = 0; ch < 4; ch++) {
        s_stableMin[ch] = 0xFFFF;
        s_stableMax[ch] = 0;
    }
#if defined(CONFIG_DEVICE_AD_4CH)
    uint8_t p[4];
    p[0] = OP_SET_DA;
    p[1] = kRefCh;
    uint16_t g = kGridLive[s_sweepIdx];
    std::memcpy(p + 2, &g, 2);
    CalibLinkSend(p, 4);
    s_phase = PH_WAIT;
#else
    for (int ch = 0; ch < 4; ch++) {
        uint16_t raw = kGridLive[s_sweepIdx];
        // CHK 比较的是「设定 ideal」与「量得 ideal」，驱动值先经本机曲线反算成 raw
        if (kCheckMode) raw = Bd().CalibDaRaw((uint8_t)ch, raw);
        Bd().CalibDaSet((uint8_t)ch, raw);
    }
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
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) == 0) {
#if defined(CONFIG_DEVICE_AD_4CH)
            if (len >= 1 && payload[0] == OP_ACK_SET_DA) {
                s_waitMs = 0;
                s_phase = PH_SETTLE;
            }
#else
            if (len >= 17 && payload[0] == OP_ACK_READ_AD) {
                for (int ch = 0; ch < 4; ch++) {
                    float v;
                    std::memcpy(&v, payload + 1 + 4 * ch, 4);
                    s_samples[ch][s_sweepIdx] = v;
                }
                if (kCheckMode) {
                    char raw[96];
                    snprintf(raw, sizeof(raw), "calib: raw m=%.1f,%.1f,%.1f,%.1f", (double)s_samples[0][s_sweepIdx],
                             (double)s_samples[1][s_sweepIdx], (double)s_samples[2][s_sweepIdx],
                             (double)s_samples[3][s_sweepIdx]);
                    LogInfo << raw;
                    ChkPoint();
                }
                AdvancePoint();
            }
#endif
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
#if defined(CONFIG_DEVICE_AD_4CH)
        s_phase = PH_SEND;
#else
        uint8_t p[1] = {OP_READ_AD};
        CalibLinkSend(p, 1);
#endif
        return;
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
            s_samples[ch][s_sweepIdx] = 0.5f * ((float)s_stableMin[ch] + (float)s_stableMax[ch]);
        }
        {
            char m[112];
            snprintf(m, sizeof(m), "calib: pt%u ideal=%u raw=%u,%u,%u,%u%s", (unsigned)s_sweepIdx,
                     (unsigned)kGridLive[s_sweepIdx], (unsigned)s_samples[0][s_sweepIdx],
                     (unsigned)s_samples[1][s_sweepIdx], (unsigned)s_samples[2][s_sweepIdx],
                     (unsigned)s_samples[3][s_sweepIdx], settled ? "" : " unstable");
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
        s_waitMs++;
        if (s_waitMs < kSettleMs) return;
        uint8_t p[1] = {OP_READ_AD};
        CalibLinkSend(p, 1);
        s_retry = 0;
        s_waitMs = 0;
        s_phase = PH_WAIT;
#endif
        return;
    }
}

void AdvVerifyPoint();

void ChkVerifyPoint() {
    const double set = (double)kGridLive[s_verifyIdx];
    char msg[128];
    int n = snprintf(msg, sizeof(msg), "calib: vfy set=%u d=", (unsigned)kGridLive[s_verifyIdx]);
    for (int ch = 0; ch < 4; ch++) {
        double d = PolyEval(s_rec.coeff[ch], (double)s_verify[ch][s_verifyIdx]) - set;
        s_verSumSq[ch] += d * d;
        double ad = d < 0 ? -d : d;
        if (ad > s_verMax[ch]) {
            s_verMax[ch] = ad;
            s_verMaxAt[ch] = kGridLive[s_verifyIdx];
        }
        n += snprintf(msg + n, sizeof(msg) - n, "%s%+.1f", ch ? "," : "", d);
    }
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
        uint8_t p[4];
        p[0] = OP_SET_DA;
        p[1] = kRefCh;
        uint16_t ideal = kGridLive[s_verifyIdx];
        std::memcpy(p + 2, &ideal, 2);
        CalibLinkSend(p, 4);
        s_phase = PH_WAIT;
        return;
    }

    if (s_phase == PH_WAIT) {
        uint8_t payload[40];
        uint8_t len;
        if (CalibLinkRecv(payload, &len) == 0) {
            if (len >= 1 && payload[0] == OP_ACK_SET_DA) {
                s_waitMs = 0;
                s_phase = PH_SETTLE;
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
    }

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
    bool settled = true;
    for (int ch = 0; ch < 4; ch++) {
        if (s_stableMax[ch] - s_stableMin[ch] > kStableCode) settled = false;
        s_verify[ch][s_verifyIdx] = 0.5f * ((float)s_stableMin[ch] + (float)s_stableMax[ch]);
    }
    {
        char m[112];
        snprintf(m, sizeof(m), "calib: vfy%u set=%u raw=%u,%u,%u,%u%s", (unsigned)s_verifyIdx,
                 (unsigned)kGridLive[s_verifyIdx], (unsigned)s_verify[0][s_verifyIdx], (unsigned)s_verify[1][s_verifyIdx],
                 (unsigned)s_verify[2][s_verifyIdx], (unsigned)s_verify[3][s_verifyIdx], settled ? "" : " unstable");
        LogInfo << m;
    }
#if defined(CALIB_LOG_SPAN)
    {
        char sp[128];
        snprintf(sp, sizeof(sp), "calib: vspan%u min=%u,%u,%u,%u max=%u,%u,%u,%u", (unsigned)s_verifyIdx,
                 (unsigned)s_stableMin[0], (unsigned)s_stableMin[1], (unsigned)s_stableMin[2],
                 (unsigned)s_stableMin[3], (unsigned)s_stableMax[0], (unsigned)s_stableMax[1],
                 (unsigned)s_stableMax[2], (unsigned)s_stableMax[3]);
        LogInfo << sp;
    }
#endif
    ChkVerifyPoint();
    AdvVerifyPoint();
}

void AdvVerifyPoint() {
    s_verifyIdx++;
    s_stableCount = -1;
    if (s_verifyIdx >= kGridN) {
        s_state = S_VERIFY;
        return;
    }
    s_phase = PH_SEND;
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

    char msg[128];
    int n = snprintf(msg, sizeof(msg), "calib: verify");
    bool pass = true;
    for (int ch = 0; ch < 4; ch++) {
        double rmse = sqrt(s_verSumSq[ch] / kGridN);
        n += snprintf(msg + n, sizeof(msg) - n, "%s ch%d max=%.1f @set=%u rmse=%.1f", ch ? ";" : "", ch, s_verMax[ch],
                      (unsigned)s_verMaxAt[ch], rmse);
        if (s_verMax[ch] >= kCheckGateCode) pass = false;
    }
    LogInfo << msg;

    bool anyBad = false;
    for (int ch = 0; ch < 4; ch++) {
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

// 导出采样点与系数供 PC 端复算；每 tick 只发一行，且行间留出间隔让 RTT 主机读走
void DoDump() {
    enum { kChunkVals = 24, kStr = 200, kLineGapMs = 100 };
    if (s_dumpWait > 0) {
        s_dumpWait--;
        return;
    }
    const int perCh = (kGridN + kChunkVals - 1) / kChunkVals;
    const int gridLines = perCh;
    const int chLines = perCh * 4;
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
        int ch = j / perCh;
        int off = (j % perCh) * kChunkVals;
        int n = snprintf(dl, sizeof(dl), "calib: dump ch%d %d", ch, off);
        for (int i = off; i < kGridN && i < off + kChunkVals; i++) {
            n += snprintf(dl + n, sizeof(dl) - n, " %u", (unsigned)s_samples[ch][i]);
        }
        LogInfo << dl;
        s_dumpLine++;
        s_dumpWait = kLineGapMs;
        return;
    }

    if (s_dumpLine < gridLines + chLines + 4) {
        int ch = s_dumpLine - gridLines - chLines;
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

void DoFit() {
    double xs[kGridN];
    for (int ch = 0; ch < 4; ch++) {
        for (int i = 0; i < kGridN; i++) {
            xs[i] = (double)s_samples[ch][i];
        }
        if (!PolyFit(xs, s_gridD, kGridN, s_coeff[ch])) {
            LogError << "calib: fit failed ch=" << ch;
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
        snprintf(st, sizeof(st), "calib: ch%d resid max=%.1f @ideal=%u rmse=%.1f",
                 ch, maxRes, (unsigned)kGridLive[maxAt], sqrt(sumSq / kGridN));
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
    }

    for (int i = 0; i < kGridN; i++) {
        xs[i] = (double)s_samples[kRefCh][i];
    }
    double dev = 0.0;
    for (int i = 0; i < kGridN; i++) {
#if defined(CONFIG_DEVICE_AD_4CH)
        double builtin = Bd().CalibBuiltinIdealFromRaw((uint16_t)xs[i]);
#else
        double builtin = Bd().CalibBuiltinRawFromIdeal((uint16_t)xs[i]);
#endif
        double d = PolyEval(s_coeff[kRefCh], xs[i]) - builtin;
        if (d < 0) d = -d;
        if (d > dev) dev = d;
    }
    char devMsg[48];
    snprintf(devMsg, sizeof(devMsg), "calib: ch%u anchor dev = %g", (unsigned)kRefCh, dev);
    LogInfo << devMsg;

    s_dumpLine = 0;
    s_dumpWait = 0;
    s_state = S_DUMP;
}

void DoSave() {
    CalibRecord rec;
#if defined(CONFIG_DEVICE_AD_4CH)
    rec.board = 1;
#else
    rec.board = 2;
#endif
    rec.degree = POLY_DEGREE;
    rec.channelCount = 4;
    for (int ch = 0; ch < 4; ch++) {
        for (int k = 0; k < POLY_COEFF_NUM; k++) {
            rec.coeff[ch][k] = s_coeff[ch][k];
        }
    }
    if (!CalibStoreSave(&rec)) {
        LogError << "calib: flash write failed";
        EnterFail();
        return;
    }
    s_rec = rec;
    LogInfo << "calib: saved";
    s_verifyIdx = 0;
    s_stableCount = -1;
    s_verifyReported = false;
    s_phase = PH_SEND;
    s_state = S_VERIFY;
}

#if defined(CONFIG_CALIB_ROLE_CHK)
void DoCheck() {
    char msg[80];
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
    switch (s_state) {
        case S_PING: {
            uint8_t payload[40];
            uint8_t len;
            if (CalibLinkRecv(payload, &len) == 0 && len >= 1 && payload[0] == OP_PONG) {
                if (kCheckMode) {
                    LogInfo << "calib: role CHK";
                } else {
                    LogInfo << "calib: role DUT";
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
            DoFit();
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
        case S_VERIFY:
            if (!s_verifyReported) {
                DoVerify();
            } else if (s_verifyIdx >= kGridN) {
                DoVerify();
            } else {
                DoVerifySweep();
            }
            break;
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
    CalibLinkInit();
#if defined(CALIB_FIXED_IDEAL)
    for (int i = 0; i < kGridN; i++) {
        kGridLive[i] = (uint16_t)(CALIB_FIXED_IDEAL);
    }
#endif
    for (int i = 0; i < kGridN; i++) {
        s_gridD[i] = (double)kGridLive[i];
    }

#if defined(CONFIG_CALIB_ROLE_REF)
    LogInfo << "calib: role REF ready";
    LedSet(true);
#else
    s_state = S_PING;
    s_pingCount = 0;
    s_pingElapsed = kPingIntervalMs;
#endif

    bool loaded = CalibStoreLoad(&s_rec);
    if (loaded) {
        LogInfo << "calib: loaded";
    }
#if defined(CONFIG_VERIFY_ON_BOOT)
    if (loaded) {
        s_state = S_VERIFY;
        s_verifyIdx = 0;
        s_stableCount = -1;
        s_verifyReported = false;
    }
#endif
}

void CalibProcTick() {
#if defined(CONFIG_CALIB_ROLE_REF)
    RefTick();
#else
    DutTick();
#endif
}
