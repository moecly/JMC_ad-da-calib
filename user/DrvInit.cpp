#include "DrvInit.h"

void DrvInit::PinInit(gpio_type *port, uint32_t pin, gpio_output_type outputType, gpio_pull_type pullType, gpio_mode_type modeType, gpio_drive_type driveType) {
    gpio_init_type gpioInitStruct;
    gpio_default_para_init(&gpioInitStruct);
    gpioInitStruct.gpio_pins = pin;
    gpioInitStruct.gpio_out_type = outputType;
    gpioInitStruct.gpio_pull = pullType;
    gpioInitStruct.gpio_mode = modeType;
    gpioInitStruct.gpio_drive_strength = driveType;
    gpio_init(port, &gpioInitStruct);
}

void DrvInit::Pinmux(gpio_type* port, gpio_pins_source_type gpioPinSource, gpio_mux_sel_type gpioMux) {
    gpio_pin_mux_config(port, gpioPinSource, gpioMux);
}

void DrvInit::DmaInit(dma_channel_type* channel, uint16_t bufferSize, dma_dir_type dir, uint32_t memBaseAddr, dma_memory_data_size_type memDataSize, confirm_state memIncEnable, uint32_t perBaseAddr, dma_peripheral_data_size_type perBaseSize, confirm_state perIncEnable, dma_priority_level_type priority, confirm_state loopModeEnable) {
    dma_init_type dmaInitStruct;

    /* dma1 channel1 for usart2 tx configuration */
    dma_reset(channel);
    dma_default_para_init(&dmaInitStruct);
    dmaInitStruct.buffer_size = bufferSize;
    dmaInitStruct.direction = dir;
    dmaInitStruct.memory_base_addr = memBaseAddr;
    dmaInitStruct.memory_data_width = memDataSize;
    dmaInitStruct.memory_inc_enable = memIncEnable;
    dmaInitStruct.peripheral_base_addr = perBaseAddr;
    dmaInitStruct.peripheral_data_width = perBaseSize;
    dmaInitStruct.peripheral_inc_enable = perIncEnable;
    dmaInitStruct.priority = priority;
    dmaInitStruct.loop_mode_enable = loopModeEnable;
    dma_init(channel, &dmaInitStruct);
}

void DrvInit::DmaFlexibleConfig(dma_type* dma, uint8_t flexChannelx, dma_flexible_request_type flexibleRequest) {
    dma_flexible_config(dma, flexChannelx, flexibleRequest);
}

void DrvInit::ClockEnable(crm_periph_clock_type clock) {
    crm_periph_clock_enable(clock, TRUE);
}

void DrvInit::UartInit(usart_type* usart, uint32_t baudRate, usart_data_bit_num_type dataBit, usart_stop_bit_num_type stopBit, usart_de_polarity_type dePolarity, bool idleIntEnable, bool rxDmaEnable, bool txDmaEnable) {
    usart_reset(usart);
    usart_init(usart, baudRate, dataBit, stopBit);
    usart_rs485_delay_time_config(usart, 0, 0);
    usart_rs485_mode_enable(usart, TRUE);
    usart_de_polarity_set(usart, dePolarity);
    usart_receiver_enable(usart, TRUE);
    usart_transmitter_enable(usart, TRUE);
    usart_enable(usart, TRUE);
    if (idleIntEnable) {
        usart_interrupt_enable(usart, USART_IDLE_INT, TRUE);
    }
    if (rxDmaEnable) {
        usart_dma_receiver_enable(usart, TRUE);
    }
    if (txDmaEnable) {
        usart_dma_transmitter_enable(usart, TRUE);
    }
}

void DrvInit::IrqEnable(IRQn_Type irqn, uint32_t preemptPriority, uint32_t subPriority) {
    nvic_irq_enable(irqn, preemptPriority, subPriority);
}

void DrvInit::IrqDisable(IRQn_Type irqn, uint32_t preemptPriority, uint32_t subPriority) {
    nvic_irq_disable(irqn);
}

void DrvInit::ExintInit(scfg_port_source_type portSource, scfg_pins_source_type pinSource, exint_line_mode_type lineMode, uint32_t lineSelect, exint_polarity_config_type linePolarity, confirm_state lineEnable) {
    exint_init_type exintInitStruct;
    scfg_exint_line_config(portSource, pinSource);
    exint_default_para_init(&exintInitStruct);
    exintInitStruct.line_enable = lineEnable;
    exintInitStruct.line_mode = lineMode;
    exintInitStruct.line_select = lineSelect;
    exintInitStruct.line_polarity = linePolarity;
    exint_init(&exintInitStruct);
}
