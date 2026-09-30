#include "SysDelay.h"

#include "at32f425_board.h"
#define STEP_DELAY_MS (uint32_t)(50)
#define TICK_COUNT_MAX (uint32_t)(0xFFFFFF)
#define TICK_COUNT_VALUE (SysTick->VAL)

// /* global variable */
// volatile uint32_t ticks_count_us;

// /**
//  * @brief  this function provides minimum delay (in microsecond).
//  * @param  delay: specifies the delay time length, in microsecond.
//  * @retval none
//  */
// static void delay_us(uint32_t delay) {
//     uint32_t delay_ticks, pre_ticks, cur_ticks, delta;
//     delay_ticks = delay * ticks_count_us;

//     pre_ticks = TICK_COUNT_VALUE;
//     do {
//         cur_ticks = TICK_COUNT_VALUE;
//         /* count down */
//         delta = (cur_ticks <= pre_ticks) ? (pre_ticks - cur_ticks) : ((TICK_COUNT_MAX - cur_ticks) + pre_ticks + 1);
//     } while (delta < delay_ticks);
// }

SysDelay::SysDelay() {
    crm_clocks_freq_type crm_clocks;
    uint32_t frequency = 0;

    /* get crm_clocks */
    crm_clocks_freq_get(&crm_clocks);

    frequency = crm_clocks.ahb_freq / 8;

    /* config systick clock source */
    systick_clock_source_config(SYSTICK_CLOCK_SOURCE_AHBCLK_DIV8);
    ticks_count_us = (frequency / 1000000U);
    /* system tick config */
    TICK_COUNT_VALUE = 0UL;
    SysTick->LOAD = TICK_COUNT_MAX;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
}

void SysDelay::DelayUs(uint32_t us) {
    uint32_t delay_ticks, pre_ticks, cur_ticks, delta;
    delay_ticks = us * ticks_count_us;

    pre_ticks = TICK_COUNT_VALUE;
    do {
        cur_ticks = TICK_COUNT_VALUE;
        /* count down */
        delta = (cur_ticks <= pre_ticks) ? (pre_ticks - cur_ticks) : ((TICK_COUNT_MAX - cur_ticks) + pre_ticks + 1);
    } while (delta < delay_ticks);
}

void SysDelay::DelayMs(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        DelayUs(1000);
    }
}

void SysDelay::DelaySec(uint32_t sec) {
    for (uint32_t i = 0; i < sec; i++) {
        DelayMs(1000);
    }
}
