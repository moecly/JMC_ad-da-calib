#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "at32f425_gpio.h"
#include "at32f425_spi.h"

class Delay;

class MS5542 {
public:
    MS5542(spi_type* spi, gpio_type* csPort, uint16_t csPin);
    virtual ~MS5542();
    std::optional<bool> SetVal(uint16_t val);

private:
    std::unique_ptr<Delay> m_delay;
    spi_type* m_spi;
    gpio_type* m_csPort;
    uint16_t m_csPin;
    uint16_t m_lastVal;
};
