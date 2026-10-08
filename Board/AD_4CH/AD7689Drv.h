#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "Delay/Delay.h"
#include "at32f425_spi.h"

class AD7689Drv {
public:
    static constexpr int NUM_CHANNELS = 4;
    static constexpr int FILTER_NUM = 32;

    AD7689Drv(spi_type* spi);
    bool Init();
    bool Sync();
    std::array<uint16_t, NUM_CHANNELS> GetAdData() const;

private:
    uint16_t ReadOneChannelData(int channel);
    uint16_t RetCfgVal(int cfg, int incc, int inx, int bw, int ref, int seq, int rb) const;
    uint16_t GetAveragerVal(const std::array<uint16_t, FILTER_NUM>& filterData) const;

    static constexpr int ADREG_CFG = 0b1;
    static constexpr int ADREG_INCC = 0b111;
    static constexpr int ADREG_BW = 0b1;
    static constexpr int ADREG_REF = 0b001;
    static constexpr int ADREG_SEQ = 0b00;
    static constexpr int ADREG_RB = 0b1;

    std::unique_ptr<Delay> m_delay;
    spi_type* m_spi;
    std::array<uint16_t, NUM_CHANNELS> m_readADData;
    std::array<std::array<uint16_t, FILTER_NUM>, NUM_CHANNELS> m_filterData;
};
