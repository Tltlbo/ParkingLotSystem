#include "apMain.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_tim.h"
#include "myHcSr04.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>



#include "mySg90.h"
// #include "myHcsr04.h" // 실제 초음파 센서 드라이버 헤더

#define DETECTION_THRESHOLD_CM  15   // 감지 임계 거리 (15cm)
#define PROCESS_PERIOD_MS       10   // 10ms 주기 (5회 연속 감지 시 50ms 디바운스 달성)


float distance_cm[HCSR04_SENSOR_COUNT] = {0.0f};


// 센서별 측정 거리 저장 변수
static uint16_t s_dist1 = 0; // 센서 1 (입구 외부)
static uint16_t s_dist2 = 0; // 센서 2 (입구 내부)
static uint16_t s_dist3 = 0; // 센서 3 (출구 내부)
static uint16_t s_dist4 = 0; // 센서 4 (출구 외부)

// 주기 제어 타이머
static uint32_t s_last_process_tick = 0;

/**
 * @brief 센서별 거리 측정 함수 (프로젝트의 실제 센서 읽기 함수로 연결)
 */
static uint16_t Read_Ultrasonic_Sensor(uint8_t sensor_idx)
{
    // 예: return HCSR04_GetDistance(sensor_idx);
    switch (sensor_idx)
    {
        case 0: return 0; // 센서1 거리 반환 로직
        case 1: return 0; // 센서2 거리 반환 로직
        case 2: return 0; // 센서3 거리 반환 로직
        case 3: return 0; // 센서4 거리 반환 로직
        default: return 0;
    }
}

void apInit(void)
{
    // 차단기 모터 및 상태 머신 초기화
    SG90_Init();
    hcSr04Init();
    
}

void apMain(void)
{
    uint32_t current_tick = HAL_GetTick();
  
     while(1)
    {
      for (uint8_t sensor_num = 0; sensor_num < HCSR04_SENSOR_COUNT; sensor_num++)
      {
        if (!hcSr04Read(&distance_cm[sensor_num], sensor_num))
        {
          distance_cm[sensor_num] = -1.0f;
        }

        printf("dis[%u] : %6.1f\r\n", sensor_num + 1, distance_cm[sensor_num]);
        HAL_Delay(10);
      }
    }


    // 10ms 주기로 센서 측정 및 차단기 FSM 동시 처리
    if (current_tick - s_last_process_tick >= PROCESS_PERIOD_MS)
    {
        s_last_process_tick = current_tick;

        // 1. 센서 4개 값 갱신
        s_dist1 = Read_Ultrasonic_Sensor(0);
        s_dist2 = Read_Ultrasonic_Sensor(1);
        s_dist3 = Read_Ultrasonic_Sensor(2);
        s_dist4 = Read_Ultrasonic_Sensor(3);

        // 2. 주차 차단기 상태 머신 실행
        SG90_ProcessParkingLane(s_dist1, s_dist2, s_dist3, s_dist4, DETECTION_THRESHOLD_CM);
    }
}




