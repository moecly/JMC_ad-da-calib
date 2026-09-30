#include "GpioLed.h"

#include "CuteLogger/CuteLogger.h"
#include "DrvInit.h"


GpioLed::GpioLed(gpio_type *port, uint16_t pin, LedPolarity polarity)
    : m_port(port), m_pin(pin), m_polarity(polarity), m_state(OFF) {
    // DrvInit::PinInit(m_port, m_pin, GPIO_OUTPUT_PUSH_PULL, GPIO_PULL_UP, GPIO_MODE_OUTPUT,
    // GPIO_DRIVE_STRENGTH_STRONGER);
    Off();
}

void GpioLed::On() {
    if (m_polarity == LOW) {
        gpio_bits_reset(m_port, m_pin);
    } else {
        gpio_bits_set(m_port, m_pin);
    }
    m_state = ON;
}

void GpioLed::Off() {
    if (m_polarity == LOW) {
        gpio_bits_set(m_port, m_pin);
    } else {
        gpio_bits_reset(m_port, m_pin);
    }
    m_state = OFF;
}

void GpioLed::Toggle() {
    LedState state = GetState();
    if (state == ON) {
        Off();
    } else {
        On();
    }
}

Led::LedState GpioLed::GetState() { return m_state; }
