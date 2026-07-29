/*
 * WS2812.c
 *
 * Addressable LED support for AT32F415 targets.
 */

#include "WS2812.h"

#include "functions.h"
#include "targets.h"

#ifdef USE_LED_STRIP

#ifdef WS2812_TIMER_DMA

#define WS2812_PERIOD_TICKS 180U
#define WS2812_T0H_TICKS 50U
#define WS2812_T1H_TICKS 101U
#define WS2812_DATA_BITS 24U
#define WS2812_RESET_SLOTS 48U
#define WS2812_BUFFER_SIZE (WS2812_DATA_BITS + WS2812_RESET_SLOTS)

static uint16_t ws2812_waveform[WS2812_BUFFER_SIZE];
static volatile uint32_t ws2812_requested_color;
static volatile uint32_t ws2812_request_sequence;
static uint32_t ws2812_handled_sequence;
static uint8_t ws2812_dma_active;

void send_LED_RGB(uint8_t red, uint8_t green, uint8_t blue)
{
    /* A naturally aligned 32-bit store is atomic on Cortex-M4. */
    ws2812_requested_color = ((uint32_t)green << 16) |
                             ((uint32_t)red << 8) | blue;
    ws2812_request_sequence++;
}

void WS2812_Service(void)
{
    if (ws2812_dma_active && dma_flag_get(DMA1_FDT2_FLAG) != RESET) {
        dma_channel_enable(DMA1_CHANNEL2, FALSE);
        dma_flag_clear(DMA1_GL2_FLAG);
        tmr_counter_enable(TMR5, FALSE);
        tmr_channel_value_set(TMR5, TMR_SELECT_CHANNEL_3, 0);
        ws2812_dma_active = 0;
    }

    if (!ws2812_dma_active &&
        ws2812_handled_sequence != ws2812_request_sequence) {
        uint32_t sequence;
        uint32_t color;
        do {
            sequence = ws2812_request_sequence;
            color = ws2812_requested_color;
        } while (sequence != ws2812_request_sequence);
        ws2812_handled_sequence = sequence;

        for (uint8_t i = 0; i < WS2812_DATA_BITS; i++) {
            ws2812_waveform[i] = (color & (1UL << (23U - i))) ?
                WS2812_T1H_TICKS : WS2812_T0H_TICKS;
        }
        for (uint8_t i = WS2812_DATA_BITS; i < WS2812_BUFFER_SIZE; i++) {
            ws2812_waveform[i] = 0;
        }

        dma_channel_enable(DMA1_CHANNEL2, FALSE);
        dma_flag_clear(DMA1_GL2_FLAG);
        DMA1_CHANNEL2->maddr = (uint32_t)ws2812_waveform;
        dma_data_number_set(DMA1_CHANNEL2, WS2812_BUFFER_SIZE);
        TMR5->cval = 0;
        tmr_channel_value_set(TMR5, TMR_SELECT_CHANNEL_3, 0);
        dma_channel_enable(DMA1_CHANNEL2, TRUE);
        ws2812_dma_active = 1;
        tmr_counter_enable(TMR5, TRUE);
    }
}

void WS2812_Init(void)
{
    dma_init_type dma_init_struct;
    tmr_output_config_type output_config;

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_TMR5_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    gpio_pin_remap_config(TMR5_GMUX_001, TRUE);
    gpio_mode_QUICK(WS2812_GPIO_PORT, GPIO_MODE_MUX, GPIO_PULL_NONE,
        WS2812_PIN);

    tmr_base_init(TMR5, WS2812_PERIOD_TICKS - 1U, 0);
    tmr_output_default_para_init(&output_config);
    output_config.oc_mode = TMR_OUTPUT_CONTROL_PWM_MODE_A;
    output_config.oc_polarity = TMR_OUTPUT_ACTIVE_HIGH;
    output_config.oc_output_state = TRUE;
    tmr_output_channel_config(TMR5, TMR_SELECT_CHANNEL_3, &output_config);
    tmr_output_channel_buffer_enable(TMR5, TMR_SELECT_CHANNEL_3, FALSE);
    tmr_channel_value_set(TMR5, TMR_SELECT_CHANNEL_3, 0);
    tmr_dma_request_enable(TMR5, TMR_OVERFLOW_DMA_REQUEST, TRUE);

    dma_flexible_config(DMA1, FLEX_CHANNEL2, DMA_FLEXIBLE_TMR5_OVERFLOW);
    dma_reset(DMA1_CHANNEL2);
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.buffer_size = WS2812_BUFFER_SIZE;
    dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_base_addr = (uint32_t)ws2812_waveform;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_HALFWORD;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_base_addr = (uint32_t)&TMR5->c3dt;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_MEDIUM;
    dma_init_struct.loop_mode_enable = FALSE;
    dma_init(DMA1_CHANNEL2, &dma_init_struct);
}

#else

static void waitClockCycles(uint16_t cycles)
{
    UTILITY_TIMER->cval = 0;
    while (UTILITY_TIMER->cval < cycles) {
    }
}

static void sendBit(uint8_t bit)
{
    WS2812_GPIO_PORT->scr = WS2812_PIN;
    waitClockCycles(CPU_FREQUENCY_MHZ >> (2 - bit));
    WS2812_GPIO_PORT->clr = WS2812_PIN;
    waitClockCycles(CPU_FREQUENCY_MHZ >> (1 + bit));
}

void send_LED_RGB(uint8_t red, uint8_t green, uint8_t blue)
{
    const uint32_t color = ((uint32_t)green << 16) |
                           ((uint32_t)red << 8) | blue;

    __disable_irq();
    UTILITY_TIMER->div = 0;
    UTILITY_TIMER->swevt |= TMR_OVERFLOW_SWTRIG;

    for (uint8_t i = 0; i < 24; i++) {
        sendBit((color >> (23 - i)) & 1U);
    }

    WS2812_GPIO_PORT->clr = WS2812_PIN;
    UTILITY_TIMER->div = CPU_FREQUENCY_MHZ - 1;
    UTILITY_TIMER->swevt |= TMR_OVERFLOW_SWTRIG;
    __enable_irq();
}

void WS2812_Init(void)
{
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    gpio_mode_QUICK(WS2812_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PULL_NONE,
        WS2812_PIN);
    WS2812_GPIO_PORT->clr = WS2812_PIN;
}

void WS2812_Service(void)
{
}

#endif

#endif
