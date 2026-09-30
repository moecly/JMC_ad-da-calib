#pragma once

#include <cstdint>

class Timer {
public:
    enum TimeUnit {
        NS,
        US,
        MS,
        SEC,
    };

    Timer() = default;
    virtual ~Timer() = default;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void SetTimerPr(uint32_t prVal, TimeUnit timeUnit) = 0;
    void SetPr(uint32_t prVal, TimeUnit timeUnit);

private:
    uint32_t m_prVal;
    TimeUnit m_timeUnit;
};
