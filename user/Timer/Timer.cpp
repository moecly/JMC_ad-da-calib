#include "Timer.h"

void Timer::SetPr(uint32_t prVal, TimeUnit timeUnit) {
    m_prVal = prVal;
    m_timeUnit = timeUnit;
}
