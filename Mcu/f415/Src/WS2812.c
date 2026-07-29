/*
 * WS2812.c
 *
 * Addressable LED support for AT32F415 targets.
 */

#include "WS2812.h"

#include "functions.h"
#include "targets.h"

#ifdef USE_LED_STRIP

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

#endif
