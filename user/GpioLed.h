#pragma once

#include "Led/Led.h"
#include "at32f425.h"

class GpioLed : public Led {
public:
    enum LedPolarity {
        LOW,  // 低电平点亮
        HIGH, // 高电平点亮
    };

    GpioLed(gpio_type *port, uint16_t pin, LedPolarity polarity);
    void On() override;
    void Off() override;
    void Toggle() override;
    LedState GetState() override;

private:
    gpio_type *m_port; 
    uint16_t m_pin;
    LedPolarity m_polarity;
    LedState m_state;
};
