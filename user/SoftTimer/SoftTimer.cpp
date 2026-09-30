#include "SoftTimer.h"

SoftTimer::SoftTimer()
 : m_sysTick(0),
   m_head(new Timer()) {
    m_head->next = nullptr;
}

SoftTimer::~SoftTimer() {
    delete m_head;
}

SoftTimer& SoftTimer::GetInstance() {
    static SoftTimer* instance = nullptr;
    if (instance == nullptr) instance = new SoftTimer();
    return *instance;
}

void SoftTimer::Tick(uint32_t tick) {
    m_sysTick += tick;
}

void SoftTimer::Loop() {
    Timer* tmr = m_head;
    Timer* next = tmr->next;
    while (next) {
        uint32_t currSysTick = m_sysTick;
        if (next->currTick + next->timeout <= currSysTick) {
            next->currTick += next->timeout;
            if (next->func) { next->func(); }
            if (next->mode == TimerMode::ONESHOT) {
                tmr->next = next->next;
                next = tmr->next;
                continue;
            }
        }
        tmr = tmr->next;
        next = tmr->next;
    }
}

bool SoftTimer::Push(Timer* timer) {
    if (timer == nullptr) return false;
    Timer* tmr = m_head;
    while (tmr->next) {
        if (tmr->next == timer) return false;
        tmr = tmr->next;
    }
    tmr->next = timer;
    return true;
}

bool SoftTimer::Pop(Timer* timer) {
    if (timer == nullptr) return false;
    Timer* tmr = m_head;
    while (tmr->next) {
        if (tmr->next == timer) {
            Timer* tmp = tmr->next;
            tmr->next = tmr->next->next;
            tmp->next = nullptr;
            return true;
        }
        tmr = tmr->next;
    }
    return false;
}

void SoftTimerTick(uint32_t tick) {
    SoftTimer::GetInstance().Tick(tick);
}

void SoftTimerLoop() {
    SoftTimer::GetInstance().Loop();
}

bool SoftTimerPush(SoftTimer::Timer* timer) {
    return SoftTimer::GetInstance().Push(timer);
}

bool SoftTimerPop(SoftTimer::Timer* timer) {
    return SoftTimer::GetInstance().Pop(timer);
}
