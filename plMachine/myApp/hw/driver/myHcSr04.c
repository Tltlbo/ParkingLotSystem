#include "myHcSr04.h"
#include "stm32f4xx_hal_gpio.h"
#include <stdint.h>

typedef struct
{
  GPIO_TypeDef *trig_port;
  uint16_t trig_pin;
  GPIO_TypeDef *echo_port;
  uint16_t echo_pin;
} HcSr04Sensor_t;

static const HcSr04Sensor_t sensors[HCSR04_SENSOR_COUNT] =
{
  {GPIOA, GPIO_PIN_6, GPIOB, GPIO_PIN_1},
  {GPIOA, GPIO_PIN_7, GPIOB, GPIO_PIN_15},
  {GPIOA, GPIO_PIN_9, GPIOB, GPIO_PIN_14},
  {GPIOA, GPIO_PIN_8, GPIOB, GPIO_PIN_13}
};

static float latest_distance[HCSR04_SENSOR_COUNT] = {0.0f};

/* DWT Cycle Counter 기반 마이크로초 딜레이 */
static void dwtInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delayUs(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000);
  while ((DWT->CYCCNT - start) < ticks);
}

void hcSr04Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  dwtInit();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 | GPIO_PIN_13| GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);

}

/**
  * @brief  HC-SR04 초음파 센서로 거리를 측정 (단위: cm)
  * @param  distance_cm: 측정된 거리 값을 저장할 포인터
  * @retval true: 성공, false: 타임아웃 또는 측정 범위 초과
  */
bool hcSr04Read(float *distance_cm, uint8_t num)
{
  uint32_t timeout = 0;
  uint32_t start_tick = 0;
  uint32_t stop_tick = 0;
  const HcSr04Sensor_t *sensor;

  if (num >= HCSR04_SENSOR_COUNT)
  {
    return false;
  }
  sensor = &sensors[num];


  /* 1. Trig 핀에 10us HIGH 펄스 인가 */
  HAL_GPIO_WritePin(sensor->trig_port, sensor->trig_pin, GPIO_PIN_RESET);
  delayUs(2);
  HAL_GPIO_WritePin(sensor->trig_port, sensor->trig_pin, GPIO_PIN_SET);
  delayUs(10);
  HAL_GPIO_WritePin(sensor->trig_port, sensor->trig_pin, GPIO_PIN_RESET);

  /* 2. Echo 핀이 HIGH가 될 때까지 대기 (최대 5ms 타임아웃) */
  timeout = 50000;
  while (HAL_GPIO_ReadPin(sensor->echo_port, sensor->echo_pin) == GPIO_PIN_RESET)
  {
    if (--timeout == 0)
    {
      return false;
    }
  }

  /* 3. Echo HIGH 시작 시점 기록 */
  start_tick = DWT->CYCCNT;

  /* 4. Echo 핀이 LOW가 될 때까지 대기 (최대 약 30ms = 500cm 범위 타임아웃) */
  timeout = 300000;
  while (HAL_GPIO_ReadPin(sensor->echo_port, sensor->echo_pin) == GPIO_PIN_SET)
  {
    if (--timeout == 0)
    {
      return false;
    }
  }
  stop_tick = DWT->CYCCNT;

  /* 5. 펄스 지속 시간(us) 계산 및 거리(cm) 환산 (음속 340m/s: 시간(us) / 58.0) */
  uint32_t elapsed_ticks = stop_tick - start_tick;
  float duration_us = (float)elapsed_ticks / (float)(SystemCoreClock / 1000000);
  float dist = duration_us / 58.0f;

  /* 유효 거리 범위 체크 (2cm ~ 400cm) */
  if (dist < 2.0f || dist > 400.0f)
  {
    return false;
  }

  latest_distance[num] = dist;
  if (distance_cm)
  {
    *distance_cm = dist;
  }

  return true;
}

float hcSr04GetDistance(uint8_t num)
{
  if (num >= HCSR04_SENSOR_COUNT)
  {
    return 0.0f;
  }

  return latest_distance[num];
}
