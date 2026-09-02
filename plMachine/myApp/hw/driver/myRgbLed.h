#pragma once

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * RGB LED 공통 극성 설정:
 * 1: Common Cathode (GND 공통, High 인가 시 점등)
 * 0: Common Anode (VCC 공통, Low 인가 시 점등)
 */
#define RGB_ACTIVE_LEVEL   1

typedef enum {
    RGB_LED_1 = 0, // 1번 LED (입구 게이트 측): Red(PA0), Green(PA1), Blue(PA4)
    RGB_LED_2,     // 2번 LED (출구 게이트 측): Red(PB0), Green(PC1), Blue(PC0)
    RGB_LED_COUNT
} RgbLed_Id_t;

typedef enum {
    LED_COLOR_OFF = 0,
    LED_COLOR_GREEN,            // 기본 대기 상태 (녹색)
    LED_COLOR_RED,              // 정지/진입금지 (적색)
    LED_COLOR_YELLOW,           // 황색 (적색 + 녹색 동시 점등)
    LED_COLOR_ORANGE = LED_COLOR_YELLOW,
    LED_COLOR_BLUE,             // 청색
    LED_COLOR_BLINK_RED_YELLOW, // 경고 모드: 적색 <-> 황색 교차 점등
    LED_COLOR_BLINK_RED_ORANGE = LED_COLOR_BLINK_RED_YELLOW
} RgbLed_Color_t;

void RgbLed_Init(void);
void RgbLed_SetColor(RgbLed_Id_t id, RgbLed_Color_t color);
RgbLed_Color_t RgbLed_GetColor(RgbLed_Id_t id);
void RgbLed_Update(uint32_t now_ms);
