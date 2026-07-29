/*
 * WS2812.h
 *
 * Addressable LED support for AT32F415 targets.
 */

#ifndef INC_WS2812_H_
#define INC_WS2812_H_

#include "main.h"

void WS2812_Init(void);
void send_LED_RGB(uint8_t red, uint8_t green, uint8_t blue);
void WS2812_Service(void);

#endif /* INC_WS2812_H_ */
