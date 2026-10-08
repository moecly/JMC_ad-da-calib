#include "AD7689Drv.h"

#include <cstdint>

#include "CuteLogger/CuteLogger.h"
#include "SysDelay.h"
#include "at32f425_board.h"
#include "at32f425_spi.h"

AD7689Drv::AD7689Drv(spi_type* spi) : m_spi(spi) {
    m_readADData = {};
    m_delay = std::make_unique<SysDelay>();
}

bool AD7689Drv::Init() {
    // 哑转换
    while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
    gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    spi_i2s_data_transmit(m_spi, RetCfgVal(ADREG_CFG, ADREG_INCC, 6, ADREG_BW, ADREG_REF, ADREG_SEQ, ADREG_RB));
    while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
    // 必须读走 RX 才能清 RDBF，否则后续 while(RDBF==0) 立即通过，CNV 会在帧未结束时提前拉高
    (void)spi_i2s_data_receive(m_spi);
    gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    m_delay->DelayMs(1);

    while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
    gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    spi_i2s_data_transmit(m_spi, RetCfgVal(ADREG_CFG, ADREG_INCC, 6, ADREG_BW, ADREG_REF, ADREG_SEQ, ADREG_RB));
    while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
    (void)spi_i2s_data_receive(m_spi);
    gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    m_delay->DelayMs(1);
    return true;
}

bool AD7689Drv::Sync() {
    // CFG 滞后两个转换窗口：本帧读到的数据属于两帧前写入的通道（手册 one deep delay）
    static int cnt = 0;
    static int cnt1 = 0;
    // static int cnt2 = 0;
    // cnt2++;
    // if (cnt2 < 2) {
    //     return true;
    // }
    // cnt2 = 0;

    if (cnt == 0) {
        while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
        gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
        // m_delay->DelayUs(50);
        spi_i2s_data_transmit(m_spi, RetCfgVal(ADREG_CFG, ADREG_INCC, 2, ADREG_BW, ADREG_REF, ADREG_SEQ, ADREG_RB));
        while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
        // m_filterData[3] <- IN7
#if 0
        m_readADData[3] = spi_i2s_data_receive(m_spi);
#else
        m_filterData[3][cnt1] = spi_i2s_data_receive(m_spi);
#endif
        // m_delay->DelayUs(50);
        gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    }

    if (cnt == 1) {
        while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
        gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
        // m_delay->DelayUs(50);
        spi_i2s_data_transmit(m_spi, RetCfgVal(ADREG_CFG, ADREG_INCC, 5, ADREG_BW, ADREG_REF, ADREG_SEQ, ADREG_RB));
        while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
        // m_filterData[0] <- IN0
#if 0
        m_readADData[0] = spi_i2s_data_receive(m_spi);
#else
        m_filterData[0][cnt1] = spi_i2s_data_receive(m_spi);
#endif
        // m_delay->DelayUs(50);
        gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    }

    if (cnt == 2) {
        while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
        gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
        // m_delay->DelayUs(50);
        spi_i2s_data_transmit(m_spi, RetCfgVal(ADREG_CFG, ADREG_INCC, 7, ADREG_BW, ADREG_REF, ADREG_SEQ, ADREG_RB));
        while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
        // m_filterData[1] <- IN2
#if 0
        m_readADData[1] = spi_i2s_data_receive(m_spi);
#else
        m_filterData[1][cnt1] = spi_i2s_data_receive(m_spi);
#endif
        // m_delay->DelayUs(50);
        gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    }

    if (cnt == 3) {
        while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
        gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
        // m_delay->DelayUs(50);
        spi_i2s_data_transmit(m_spi, RetCfgVal(ADREG_CFG, ADREG_INCC, 0, ADREG_BW, ADREG_REF, ADREG_SEQ, ADREG_RB));
        while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
        // m_filterData[2] <- IN5
#if 0
        m_readADData[2] = spi_i2s_data_receive(m_spi);
#else
        m_filterData[2][cnt1] = spi_i2s_data_receive(m_spi);
#endif
        // m_delay->DelayUs(50);
        gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    }

    cnt++;
    if (cnt >= 4) {
        cnt = 0;
#if 0
#else
        cnt1++;
        if (cnt1 == FILTER_NUM) {
            cnt1 = 0;
        }
        for (int i = 0; i < NUM_CHANNELS; i++) {
            m_readADData[i] = GetAveragerVal(m_filterData[i]);
        }
        // }
#endif
    }

    // for (int i = 0; i < AD7689Drv::NUM_CHANNELS; i++) {
    // for (int i = 0; i < 1; i++) {
    //     m_readADData[i] = ReadOneChannelData(i);
    // }
    // m_readADData[0] = ReadOneChannelData(0);
    // m_readADData[1] = ReadOneChannelData(2);
    // m_readADData[2] = ReadOneChannelData(5);
    // m_readADData[3] = ReadOneChannelData(7);

    // LogDebug << "m_readADData[0] = " << m_readADData[0];

    return true;
}

std::array<uint16_t, AD7689Drv::NUM_CHANNELS> AD7689Drv::GetAdData() const {
    std::array<uint16_t, AD7689Drv::NUM_CHANNELS> adData = m_readADData;
    return m_readADData;
}

uint16_t AD7689Drv::ReadOneChannelData(int channel) {
    uint16_t readVal = 0;
    uint16_t cfg = RetCfgVal(ADREG_CFG, ADREG_INCC, channel, ADREG_BW, ADREG_REF, ADREG_SEQ, ADREG_RB);

    while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
    gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    spi_i2s_data_transmit(m_spi, cfg);
    while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
    readVal = spi_i2s_data_receive(m_spi);
    gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    m_delay->DelayMs(1);

    while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
    gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    spi_i2s_data_transmit(m_spi, cfg);
    while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
    readVal = spi_i2s_data_receive(m_spi);
    gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    m_delay->DelayMs(1);

    while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
    gpio_bits_reset(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    spi_i2s_data_transmit(m_spi, cfg);
    while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
    readVal = spi_i2s_data_receive(m_spi);
    gpio_bits_set(AD_SPI_CS_GPIO_PORT, AD_SPI_CS_PIN);
    m_delay->DelayMs(1);

    // if (channel == 0) {
    // LogDebug << "channel[0] = " << readVal;
    // LogDebug << "readVal = " << readVal;
    LogDebug << "chann = " << channel << ", readVal = " << readVal;
    // }

    return readVal;
}

uint16_t AD7689Drv::RetCfgVal(int cfg, int incc, int inx, int bw, int ref, int seq, int rb) const {
    uint16_t cfgVal = 0;
    cfgVal |= (static_cast<uint16_t>(cfg & 0x1) << 13);
    cfgVal |= (static_cast<uint16_t>(incc & 0x7) << 10);
    cfgVal |= (static_cast<uint16_t>(inx & 0x7) << 7);
    cfgVal |= (static_cast<uint16_t>(bw & 0x1) << 6);
    cfgVal |= (static_cast<uint16_t>(ref & 0x7) << 3);
    cfgVal |= (static_cast<uint16_t>(seq & 0x3) << 1);
    cfgVal |= (static_cast<uint16_t>(rb & 0x1) << 0);
    return (cfgVal << 2);
}

uint16_t AD7689Drv::GetAveragerVal(const std::array<uint16_t, FILTER_NUM>& filterData) const {
    uint32_t retVal = 0;
    for (int i = 0; i < FILTER_NUM; i++) {
        retVal += filterData[i];
    }
    return retVal / FILTER_NUM;
}
