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

constexpr uint32_t kSettleMs = 200;
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
// 参考端可信通道：硬件与标定实测均为 ch0，内建曲线 code_to_code() 也对应 ch0
constexpr uint8_t kRefCh = 0;
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

enum State { S_PING, S_SWEEP, S_FIT, S_CHECK, S_SAVE, S_DONE, S_FAIL };
enum Phase { PH_SEND, PH_WAIT, PH_SETTLE };

float s_samples[4][kGridN];
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
                Bd().CalibDaSet(payload[1], Bd().CalibDaRaw(payload[1], ideal));
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
    uint16_t g = kGrid[s_sweepIdx];
    std::memcpy(p + 2, &g, 2);
    CalibLinkSend(p, 4);
    s_phase = PH_WAIT;
#else
    for (int ch = 0; ch < 4; ch++) {
        uint16_t raw = kGrid[s_sweepIdx];
        // CHK 比较的是「设定 ideal」与「量得 ideal」，驱动值先经本机曲线反算成 raw
        if (kCheckMode) raw = Bd().CalibDaRaw((uint8_t)ch, raw);
        Bd().CalibDaSet((uint8_t)ch, raw);
    }
    s_phase = PH_SETTLE;
#endif
}

void ChkPoint() {
    double d[4];
    const double set = (double)kGrid[s_sweepIdx];
    for (int ch = 0; ch < 4; ch++) {
        d[ch] = (double)s_samples[ch][s_sweepIdx] - set;
        const double ad = d[ch] < 0 ? -d[ch] : d[ch];
        if (ad > s_chkMax[ch]) {
            s_chkMax[ch] = ad;
            s_chkMaxAt[ch] = kGrid[s_sweepIdx];
        }
    }
    char msg[80];
    snprintf(msg, sizeof(msg), "calib: set=%u d=%+.1f,%+.1f,%+.1f,%+.1f", (unsigned)kGrid[s_sweepIdx], d[0], d[1], d[2],
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
                     (unsigned)kGrid[s_sweepIdx], (unsigned)s_samples[0][s_sweepIdx],
                     (unsigned)s_samples[1][s_sweepIdx], (unsigned)s_samples[2][s_sweepIdx],
                     (unsigned)s_samples[3][s_sweepIdx], settled ? "" : " unstable");
            LogInfo << m;
        }
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
                 ch, maxRes, (unsigned)kGrid[maxAt], sqrt(sumSq / kGridN));
        LogInfo << st;
        if (maxRes > kResidualLimitCode) {
            char rs[256];
            int n = snprintf(rs, sizeof(rs), "calib: ch%d resid>%d at:", ch, (int)kResidualLimitCode);
            for (int i = 0; i < kGridN && n < (int)sizeof(rs) - 16; i++) {
                double d = PolyEval(s_coeff[ch], xs[i]) - s_gridD[i];
                if (d < 0) d = -d;
                if (d > kResidualLimitCode) {
                    n += snprintf(rs + n, sizeof(rs) - n, " %u(%.0f)", (unsigned)kGrid[i], d);
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
    s_state = S_SAVE;
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
    LogInfo << "calib: done";
    s_state = S_DONE;
    LedSet(true);
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
#if defined(CONFIG_CALIB_ROLE_CHK)
        case S_CHECK:
            DoCheck();
            break;
#endif
        case S_SAVE:
            DoSave();
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
    for (int i = 0; i < kGridN; i++) {
        s_gridD[i] = (double)kGrid[i];
    }

#if defined(CONFIG_CALIB_ROLE_REF)
    LogInfo << "calib: role REF ready";
    LedSet(true);
#else
    s_state = S_PING;
    s_pingCount = 0;
    s_pingElapsed = kPingIntervalMs;
#endif

    CalibRecord r;
    if (CalibStoreLoad(&r)) {
        LogInfo << "calib: loaded";
    }
}

void CalibProcTick() {
#if defined(CONFIG_CALIB_ROLE_REF)
    RefTick();
#else
    DutTick();
#endif
}
