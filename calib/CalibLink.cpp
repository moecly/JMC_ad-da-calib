#include "calib/CalibLink.h"

#include <cstring>

#include "CRC/CRC.h"
#include "at32f425_dma.h"
#include "at32f425_usart.h"
#include "calib/CalibOps.h"
#include "calib/CalibStore.h"

namespace {
constexpr uint8_t FRAME_HEAD = CALIB_FRAME_HEAD;
constexpr int RX_BUF_LEN = 256;
constexpr int TX_BUF_LEN = 32;
constexpr int MAX_PAYLOAD = CALIB_MAX_PAYLOAD;

uint8_t s_rxBuf[RX_BUF_LEN];
uint8_t s_txBuf[TX_BUF_LEN];
uint8_t s_rd;
}  // namespace

void CalibLinkInit() {
    usart_dma_transmitter_enable(USART2, FALSE);
    dma_channel_enable(DMA1_CHANNEL2, FALSE);
    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    dma_reset(DMA1_CHANNEL1);

    dma_init_type d;
    dma_default_para_init(&d);
    d.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
    d.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
    d.memory_inc_enable = TRUE;
    d.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    d.peripheral_inc_enable = FALSE;
    d.priority = DMA_PRIORITY_LOW;
    d.loop_mode_enable = TRUE;
    dma_init(DMA1_CHANNEL1, &d);
    dma_flexible_config(DMA1, FLEX_CHANNEL1, DMA_FLEXIBLE_UART2_RX);

    DMA1_CHANNEL1->paddr = (uint32_t)&USART2->dt;
    DMA1_CHANNEL1->maddr = (uint32_t)s_rxBuf;
    DMA1_CHANNEL1->dtcnt = RX_BUF_LEN;
    dma_channel_enable(DMA1_CHANNEL1, TRUE);

    s_rd = 0;
}

int CalibLinkSend(const uint8_t* payload, uint8_t len) {
    if (len > MAX_PAYLOAD) return -1;

    s_txBuf[0] = FRAME_HEAD;
    s_txBuf[1] = len;
    std::memcpy(s_txBuf + 2, payload, len);
    uint32_t c = CalibCrc().Calculate(reinterpret_cast<const char*>(payload), len);
    std::memcpy(s_txBuf + 2 + len, &c, 4);

    int total = 2 + len + 4;
    for (int i = 0; i < total; i++) {
        while (usart_flag_get(USART2, USART_TDBE_FLAG) == RESET) {
        }
        usart_data_transmit(USART2, s_txBuf[i]);
    }
    while (usart_flag_get(USART2, USART_TDC_FLAG) == RESET) {
    }
    return 0;
}

int CalibLinkRecv(uint8_t* payload, uint8_t* len) {
    uint16_t remaining = dma_data_number_get(DMA1_CHANNEL1);
    uint8_t wr = (uint8_t)((RX_BUF_LEN - remaining) & 0xFF);

    while (s_rd != wr) {
        if (s_rxBuf[s_rd] != FRAME_HEAD) {
            s_rd = (uint8_t)((s_rd + 1) & 0xFF);
            continue;
        }
        uint16_t avail = (uint16_t)((wr - s_rd) & 0xFF);
        if (avail < 2) return -1;
        uint8_t plen = s_rxBuf[(s_rd + 1) & 0xFF];
        if (plen > MAX_PAYLOAD + 6) {
            s_rd = (uint8_t)((s_rd + 1) & 0xFF);
            continue;
        }
        uint16_t total = (uint16_t)(2 + plen + 4);
        if (avail < total) return -1;

        uint8_t tmpPayload[TX_BUF_LEN];
        for (int i = 0; i < plen; i++) {
            tmpPayload[i] = s_rxBuf[(s_rd + 2 + i) & 0xFF];
        }
        uint32_t c = 0;
        uint8_t* cb = reinterpret_cast<uint8_t*>(&c);
        for (int i = 0; i < 4; i++) {
            cb[i] = s_rxBuf[(s_rd + 2 + plen + i) & 0xFF];
        }

        if (CalibCrc().Calculate(reinterpret_cast<const char*>(tmpPayload), plen) == c) {
            std::memcpy(payload, tmpPayload, plen);
            *len = plen;
            s_rd = (uint8_t)((s_rd + total) & 0xFF);
            return 0;
        }
        s_rd = (uint8_t)((s_rd + 1) & 0xFF);
    }
    return -1;
}
