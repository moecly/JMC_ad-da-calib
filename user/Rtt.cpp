#include "Rtt.h"

namespace {

constexpr uint32_t RTT_BUF_SIZE = 1024;

struct RttBuf {
    const char* name;
    char* buf;
    uint32_t size;
    volatile uint32_t wrOff;
    volatile uint32_t rdOff;
    uint32_t flags;
};

struct RttCb {
    char id[16];
    uint32_t maxUpBuf;
    uint32_t maxDownBuf;
    RttBuf up[1];
};

char s_buf[RTT_BUF_SIZE];

}  // namespace

// 控制块布局由 SEGGER RTT 协议固定，调试器按 "SEGGER RTT" 魔数在 RAM 中定位
extern "C" {
RttCb _SEGGER_RTT = {"SEGGER RTT", 1, 0, {{"log", s_buf, RTT_BUF_SIZE, 0, 0, 0}}};
}

void RttWrite(const char* data, uint32_t len) {
    if (data == nullptr) return;
    RttBuf& b = _SEGGER_RTT.up[0];
    uint32_t wr = b.wrOff;
    const uint32_t rd = b.rdOff;
    for (uint32_t i = 0; i < len; i++) {
        const uint32_t next = (wr + 1) % RTT_BUF_SIZE;
        if (next == rd) break;
        b.buf[wr] = data[i];
        wr = next;
    }
    b.wrOff = wr;
}
