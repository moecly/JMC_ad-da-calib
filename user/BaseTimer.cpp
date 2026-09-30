#include "BaseTimer.h"

#include "CuteLogger/CuteLogger.h"
#include "DrvInit.h"

BaseTimer::BaseTimer(tmr_type* tmr)
 : m_tmr(tmr) {
    crm_clocks_freq_type crm_clocks_freq_struct = {0};
    crm_clocks_freq_get(&crm_clocks_freq_struct);
    tmr_base_init(m_tmr, 0, (crm_clocks_freq_struct.ahb_freq / 10000) - 1);
    tmr_cnt_dir_set(m_tmr, TMR_COUNT_UP);
    tmr_interrupt_enable(m_tmr, TMR_OVF_INT, TRUE);
    // DrvInit::IrqEnable(irqn, 2, 1);
}

BaseTimer::~BaseTimer() {
}

void BaseTimer::Start() {
    tmr_counter_enable(m_tmr, TRUE);
}

void BaseTimer::Stop() {
    tmr_counter_enable(m_tmr, FALSE);
}

void BaseTimer::SetTimerPr(uint32_t prVal, Timer::TimeUnit timeUnit) {
    if (timeUnit != Timer::MS) return;
    uint32_t pr = prVal * 10;
    if (pr > UINT16_MAX) return;
    if (pr != 0) {
        pr -= 1;
    }
    crm_clocks_freq_type crm_clocks_freq_struct = {0};
    crm_clocks_freq_get(&crm_clocks_freq_struct);
    tmr_base_init(m_tmr, pr, (crm_clocks_freq_struct.ahb_freq / 10000) - 1);
}
