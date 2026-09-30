#pragma once

#include <cstdint>

class SoftTimer {
public:
    typedef void (*TimerCb)(void);

    enum TimerMode {
        ONESHOT,
        PERIOD,
    };

    struct Timer {
        uint32_t timeout;
        uint32_t currTick;
        TimerMode mode;
        TimerCb func;
        Timer* next;
    };

    SoftTimer();
    ~SoftTimer();
    static SoftTimer& GetInstance();
    void Tick(uint32_t tick);
    void Loop();
    bool Push(Timer* timer);
    bool Pop(Timer* timer);

private:
    uint32_t m_sysTick;
    Timer* m_head;
};

void SoftTimerTick(uint32_t tick);
void SoftTimerLoop();
bool SoftTimerPush(SoftTimer::Timer* timer);
bool SoftTimerPop(SoftTimer::Timer* timer);
