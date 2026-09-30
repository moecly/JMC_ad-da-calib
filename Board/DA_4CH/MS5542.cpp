#include "MS5542.h"

#include "CuteLogger/CuteLogger.h"
#include "SysDelay.h"

MS5542::MS5542(spi_type* spi, gpio_type* csPort, uint16_t csPin) : m_spi(spi), m_csPort(csPort), m_csPin(csPin) {
    m_lastVal = 1;
    SetVal(0);
    m_delay = std::make_unique<SysDelay>();
}

MS5542::~MS5542() = default;

std::optional<bool> MS5542::SetVal(uint16_t val) {
    if (!m_spi || !m_csPort) return std::nullopt;
    if (m_lastVal == val) return true;
    while (spi_i2s_flag_get(m_spi, SPI_I2S_TDBE_FLAG) == 0);
    gpio_bits_reset(m_csPort, m_csPin);
    spi_i2s_data_transmit(m_spi, val);
    while (spi_i2s_flag_get(m_spi, SPI_I2S_RDBF_FLAG) == 0);
    spi_i2s_data_receive(m_spi);
    gpio_bits_set(m_csPort, m_csPin);
    m_lastVal = val;
    // LogInfo << "success";
    return true;
}
