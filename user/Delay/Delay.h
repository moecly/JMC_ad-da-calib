#pragma once

#include <cstdint>

class Delay {
public:
    Delay() = default;
    ~Delay() = default;
    virtual void DelayUs(uint32_t us) = 0;
    virtual void DelayMs(uint32_t ms) = 0;
    virtual void DelaySec(uint32_t sec) = 0;
};
