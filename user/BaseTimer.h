#pragma once

#include "Timer/Timer.h"
#include "at32f425.h"

class BaseTimer : public Timer {
public:
    BaseTimer(tmr_type* tmr);
    ~BaseTimer();
    void Start() override;
    void Stop() override;
    void SetTimerPr(uint32_t prVal, TimeUnit timeUnit) override;

private:
    tmr_type* m_tmr;
};
