#pragma once

#include "at32f425.h"

class DrvInit {
public:
    DrvInit() = default;
    ~DrvInit() = default;
    static void PinInit(gpio_type* port, uint32_t pin, gpio_output_type outputType, gpio_pull_type pullType, gpio_mode_type modeType, gpio_drive_type driveType);
    static void Pinmux(gpio_type* port, gpio_pins_source_type gpioPinSource, gpio_mux_sel_type gpioMux);
    static void DmaInit(dma_channel_type* channel, uint16_t bufferSize, dma_dir_type dir, uint32_t memBaseAddr, dma_memory_data_size_type memDataSize, confirm_state memIncEnable, uint32_t perBaseAddr, dma_peripheral_data_size_type perBaseSize, confirm_state perIncEnable, dma_priority_level_type priority, confirm_state loopModeEnable);
    static void DmaFlexibleConfig(dma_type* dma, uint8_t flexChannelx, dma_flexible_request_type flexibleRequest);
    static void ClockEnable(crm_periph_clock_type clock);
    static void UartInit(usart_type* usart, uint32_t baudRate, usart_data_bit_num_type dataBit, usart_stop_bit_num_type stopBit, usart_de_polarity_type dePolarity, bool idleIntEnable, bool rxDmaEnable, bool txDmaEnable);
    static void IrqEnable(IRQn_Type irqn, uint32_t preemptPriority, uint32_t subPriority);
    static void IrqDisable(IRQn_Type irqn, uint32_t preemptPriority, uint32_t subPriority);
    static void ExintInit(scfg_port_source_type portSource, scfg_pins_source_type pinSource, exint_line_mode_type lineMode, uint32_t lineSelect, exint_polarity_config_type linePolarity, confirm_state lineEnable);

private:
};
