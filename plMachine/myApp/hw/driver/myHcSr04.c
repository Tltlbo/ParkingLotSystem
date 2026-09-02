#include "myHcSr04.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct
{
  GPIO_TypeDef *trig_port;
  uint16_t trig_pin;
  GPIO_TypeDef *echo_port;
  uint16_t echo_pin;
} HcSr04Sensor_t;

/*
 * 센서 0: 센서1 (입구 외부) -> Trig: PA6, Echo: PB10  
 * 센서 1: 센서2 (입구 내부) -> Trig: PA7, Echo: PB15
 * 센서 2: 센서3 (출구 내부) -> Trig: PA9, Echo: PB14
 * 센서 3: 센서4 (출구 외부) -> Trig: PA8, Echo: PB13
 */
static const HcSr04Sensor_t sensors[HCSR04_SENSOR_COUNT] =
{
  {GPIOA, GPIO_PIN_6, GPIOB, GPIO_PIN_10},
  {GPIOA, GPIO_PIN_7, GPIOB, GPIO_PIN_15},
  {GPIOA, GPIO_PIN_9, GPIOB, GPIO_PIN_14},
  {GPIOA, GPIO_PIN_8, GPIOB, GPIO_PIN_13}
};

static float s_latest_distance[HCSR04_SENSOR_COUNT] = {0.0f};

/* DWT Cycle Counter 기반 마이크로초 딜레이 및 시간 측정 초기화 */
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

  /* Trig 핀 (Output) */
  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Echo 핀 (Input) */
  GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Trig 핀 초기 LOW */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
}

/**
  * @brief  HC-SR04 초음파 센서로 거리를 측정 (단위: cm)
  * @param  distance_cm: 측정된 거리 값을 저장할 포인터 (NULL 허용)
  * @param  num: 센서 번호 (0 ~ HCSR04_SENSOR_COUNT - 1)
  * @retval true: 성공, false: 타임아웃 또는 측정 범위 초과
  */
bool hcSr04Read(float *distance_cm, uint8_t num)
{
  if (num >= HCSR04_SENSOR_COUNT)
  {
    return false;
  }

  const HcSr04Sensor_t *sensor = &sensors[num];
  uint32_t ticks_per_us = SystemCoreClock / 1000000;

  /* 1. Trig 핀에 10us HIGH 펄스 인가 */
  HAL_GPIO_WritePin(sensor->trig_port, sensor->trig_pin, GPIO_PIN_RESET);
  delayUs(2);
  HAL_GPIO_WritePin(sensor->trig_port, sensor->trig_pin, GPIO_PIN_SET);
  delayUs(10);
  HAL_GPIO_WritePin(sensor->trig_port, sensor->trig_pin, GPIO_PIN_RESET);

  /* 2. Echo 핀이 HIGH가 될 때까지 대기 (최대 2ms 타임아웃) */
  uint32_t start_wait = DWT->CYCCNT;
  uint32_t max_wait_ticks = 2000 * ticks_per_us;
  while (HAL_GPIO_ReadPin(sensor->echo_port, sensor->echo_pin) == GPIO_PIN_RESET)
  {
    if ((DWT->CYCCNT - start_wait) > max_wait_ticks)
    {
      return false;
    }
  }

  /* 3. Echo HIGH 시작 시점 기록 */
  uint32_t echo_start = DWT->CYCCNT;

  /* 4. Echo 핀이 LOW가 될 때까지 대기 (최대 약 25ms = 400cm 범위 타임아웃) */
  uint32_t max_echo_ticks = 25000 * ticks_per_us;
  while (HAL_GPIO_ReadPin(sensor->echo_port, sensor->echo_pin) == GPIO_PIN_SET)
  {
    if ((DWT->CYCCNT - echo_start) > max_echo_ticks)
    {
      return false;
    }
  }
  uint32_t echo_end = DWT->CYCCNT;

  /* 5. 펄스 지속 시간(us) 및 거리(cm) 계산 (음속 340m/s: 시간(us) / 58.0) */
  uint32_t elapsed_ticks = echo_end - echo_start;
  float duration_us = (float)elapsed_ticks / (float)ticks_per_us;
  float dist = duration_us / 58.0f;

  /* 유효 거리 범위 체크 (2cm ~ 400cm) */
  if (dist < 2.0f || dist > 400.0f)
  {
    return false;
  }

  s_latest_distance[num] = dist;
  if (distance_cm != NULL)
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

  return s_latest_distance[num];
}
