///*
// * serial_telemetry.c
// *
// *  Created on: May 13, 2020
// *      Author: Alka
// */

#include "serial_telemetry.h"
#include "common.h"
#include "kiss_telemetry.h"
#include "targets.h"

#if defined(USE_PA14_TELEMETRY) && defined(HARDWARE_GROUP_AT_B)
#error "USE_PA14_TELEMETRY uses DMA1 channel 4, which conflicts with HARDWARE_GROUP_AT_B input capture"
#endif

#ifdef USE_PA14_TELEMETRY
#define TELEMETRY_USART USART2
#define TELEMETRY_USART_CLOCK CRM_USART2_PERIPH_CLOCK
#define TELEMETRY_TX_GPIO GPIOA
#define TELEMETRY_TX_GPIO_CLOCK CRM_GPIOA_PERIPH_CLOCK
#define TELEMETRY_TX_PIN GPIO_PINS_14
#define TELEMETRY_TX_PIN_SOURCE GPIO_PINS_SOURCE14
#define TELEMETRY_TX_GPIO_MUX GPIO_MUX_1
#define TELEMETRY_DMA_CHANNEL DMA1_CHANNEL4
#else
#define TELEMETRY_USART USART1
#define TELEMETRY_USART_CLOCK CRM_USART1_PERIPH_CLOCK
#define TELEMETRY_TX_GPIO GPIOB
#define TELEMETRY_TX_GPIO_CLOCK CRM_GPIOB_PERIPH_CLOCK
#define TELEMETRY_TX_PIN GPIO_PINS_6
#define TELEMETRY_TX_PIN_SOURCE GPIO_PINS_SOURCE6
#define TELEMETRY_TX_GPIO_MUX GPIO_MUX_0
#define TELEMETRY_DMA_CHANNEL DMA1_CHANNEL2
#endif

void send_telem_DMA(uint8_t bytes)
{ // set data length and enable channel to start transfer
    TELEMETRY_DMA_CHANNEL->ctrl_bit.chen = FALSE;
    TELEMETRY_DMA_CHANNEL->dtcnt = bytes;
    TELEMETRY_DMA_CHANNEL->ctrl_bit.chen = TRUE;
}

void telem_UART_Init(void)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(TELEMETRY_USART_CLOCK, TRUE);
    crm_periph_clock_enable(TELEMETRY_TX_GPIO_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    /* configure the telemetry tx pin */
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = TELEMETRY_TX_PIN;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init(TELEMETRY_TX_GPIO, &gpio_init_struct);
    gpio_pin_mux_config(
        TELEMETRY_TX_GPIO, TELEMETRY_TX_PIN_SOURCE, TELEMETRY_TX_GPIO_MUX);

    dma_reset(TELEMETRY_DMA_CHANNEL);

    dma_init_type dma_init_struct;
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.buffer_size = sizeof(aTxBuffer);
    dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_base_addr = (uint32_t)aTxBuffer;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_base_addr = (uint32_t)&TELEMETRY_USART->dt;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_LOW;
    dma_init_struct.loop_mode_enable = FALSE;
    dma_init(TELEMETRY_DMA_CHANNEL, &dma_init_struct);

 //   TELEMETRY_DMA_CHANNEL->ctrl |= DMA_FDT_INT;
 //   TELEMETRY_DMA_CHANNEL->ctrl |= DMA_DTERR_INT;

    /* configure telemetry usart param */
    usart_init(TELEMETRY_USART, 115200, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(TELEMETRY_USART, TRUE);
    usart_receiver_enable(TELEMETRY_USART, TRUE);
    usart_single_line_halfduplex_select(TELEMETRY_USART, TRUE);
    usart_dma_transmitter_enable(TELEMETRY_USART, TRUE);
    usart_enable(TELEMETRY_USART, TRUE);

#ifndef USE_PA14_TELEMETRY
    nvic_irq_enable(DMA1_Channel3_2_IRQn, 3, 0);
#endif
}
