#include "myRgbLed.h"

typedef struct {
    GPIO_TypeDef *r_port;
    uint16_t     r_pin;
    GPIO_TypeDef *g_port;
    uint16_t     g_pin;
    GPIO_TypeDef *b_port;
    uint16_t     b_pin;
} RgbLedPin_t;

/*
 * 1번 LED: PA0-Red, PA1-Green, PA4-Blue
 * 2번 LED: PB0-Red, PC1-Green, PC0-Blue
 */
static const RgbLedPin_t s_led_pins[RGB_LED_COUNT] = {
    {GPIOA, GPIO_PIN_0, GPIOA, GPIO_PIN_1, GPIOA, GPIO_PIN_4}, // LED 1 (입구측)
    {GPIOB, GPIO_PIN_0, GPIOC, GPIO_PIN_1, GPIOC, GPIO_PIN_0}  // LED 2 (출구측)
};

static RgbLed_Color_t s_led_colors[RGB_LED_COUNT] = {LED_COLOR_GREEN, LED_COLOR_GREEN};

static void RgbLed_ApplyHardware(RgbLed_Id_t id, uint8_t r, uint8_t g, uint8_t b)
{
    if (id >= RGB_LED_COUNT) return;

    GPIO_PinState r_state = (r ? (RGB_ACTIVE_LEVEL ? GPIO_PIN_SET : GPIO_PIN_RESET) 
                               : (RGB_ACTIVE_LEVEL ? GPIO_PIN_RESET : GPIO_PIN_SET));
    GPIO_PinState g_state = (g ? (RGB_ACTIVE_LEVEL ? GPIO_PIN_SET : GPIO_PIN_RESET) 
                               : (RGB_ACTIVE_LEVEL ? GPIO_PIN_RESET : GPIO_PIN_SET));
    GPIO_PinState b_state = (b ? (RGB_ACTIVE_LEVEL ? GPIO_PIN_SET : GPIO_PIN_RESET) 
                               : (RGB_ACTIVE_LEVEL ? GPIO_PIN_RESET : GPIO_PIN_SET));

    HAL_GPIO_WritePin(s_led_pins[id].r_port, s_led_pins[id].r_pin, r_state);
    HAL_GPIO_WritePin(s_led_pins[id].g_port, s_led_pins[id].g_pin, g_state);
    HAL_GPIO_WritePin(s_led_pins[id].b_port, s_led_pins[id].b_pin, b_state);
}

void RgbLed_Init(void)
{
    // GPIO 포트 클럭 활성화
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    // 1번 LED 핀 초기화 (PA0, PA1, PA4)
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 2번 LED 핀 초기화 - Red: PB0
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 2번 LED 핀 초기화 - Green: PC1, Blue: PC0
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // 기본 상태: 양쪽 모두 GREEN 설정 (Red:OFF, Green:ON, Blue:OFF)
    for (uint8_t i = 0; i < RGB_LED_COUNT; i++) {
        s_led_colors[i] = LED_COLOR_GREEN;
        RgbLed_ApplyHardware((RgbLed_Id_t)i, 0, 1, 0);
    }
}

void RgbLed_SetColor(RgbLed_Id_t id, RgbLed_Color_t color)
{
    if (id >= RGB_LED_COUNT) return;
    s_led_colors[id] = color;
}

RgbLed_Color_t RgbLed_GetColor(RgbLed_Id_t id)
{
    if (id >= RGB_LED_COUNT) return LED_COLOR_OFF;
    return s_led_colors[id];
}

void RgbLed_Update(uint32_t now_ms)
{
    /*
     * 300ms 주기 적색 <-> 황색(Yellow) 교차 점등:
     * - 0 ~ 299ms   : [적색] Red=1, Green=0, Blue=0
     * - 300 ~ 599ms : [황색] Red=1, Green=1, Blue=0 (Red와 Green이 동시 점등되어 선명한 황색/노란색)
     */
    uint8_t is_yellow_phase = ((now_ms % 600) >= 300);

    for (uint8_t i = 0; i < RGB_LED_COUNT; i++)
    {
        switch (s_led_colors[i])
        {
            case LED_COLOR_GREEN:
                // 기본 대기 상태: 순수 녹색 (R=0, G=1, B=0)
                RgbLed_ApplyHardware((RgbLed_Id_t)i, 0, 1, 0);
                break;

            case LED_COLOR_RED:
                // 순수 적색 (R=1, G=0, B=0)
                RgbLed_ApplyHardware((RgbLed_Id_t)i, 1, 0, 0);
                break;

            case LED_COLOR_YELLOW:
                // 순수 황색 (R=1, G=1, B=0)
                RgbLed_ApplyHardware((RgbLed_Id_t)i, 1, 1, 0);
                break;

            case LED_COLOR_BLINK_RED_YELLOW:
                // 경고 모드: 적색 <-> 황색(Red+Green) 교차 점등
                if (is_yellow_phase) {
                    RgbLed_ApplyHardware((RgbLed_Id_t)i, 1, 1, 0); // 황색 (Red + Green ON)
                } else {
                    RgbLed_ApplyHardware((RgbLed_Id_t)i, 1, 0, 0); // 적색 (Red ON, Green OFF)
                }
                break;

            case LED_COLOR_BLUE:
                RgbLed_ApplyHardware((RgbLed_Id_t)i, 0, 0, 1);
                break;

            case LED_COLOR_OFF:
            default:
                RgbLed_ApplyHardware((RgbLed_Id_t)i, 0, 0, 0);
                break;
        }
    }
}
