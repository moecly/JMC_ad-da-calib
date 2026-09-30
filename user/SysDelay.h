#pragma once

#include "Delay/Delay.h"

class SysDelay : public Delay {
public:
    SysDelay();
    void DelayUs(uint32_t us) override;
    void DelayMs(uint32_t ms) override;
    void DelaySec(uint32_t sec) override;
private:
    volatile uint32_t ticks_count_us;
};
