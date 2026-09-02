#include "apMain.h"
#include "mySg90.h"
#include "myHcSr04.h"
#include "myRgbLed.h"
#include <stdio.h>

#define DETECTION_THRESHOLD_CM     10   // 감지 임계 거리 (10cm)
#define SENSOR_TRIGGER_INTERVAL_MS 60   // HC-SR04 권장 트리거 인터벌 (60ms ~ 100ms, 잔여 에코 간섭 방지)
#define FSM_PROCESS_PERIOD_MS      10   // 서보 모터 스무스 이동 및 FSM 제어 주기 (10ms)
#define DEBUG_PRINT_PERIOD_MS      300  // UART 디버그 출력 주기 (300ms)

// printf 출력용 스텁 (UART 미사용 시 더미 처리)
int __io_putchar(int ch)
{
    (void)ch;
    return ch;
}


// 센서별 최신 측정 거리 저장 변수
static uint16_t s_dist1 = 0; // 센서 1 (입구 외부) - Trig: PA6, Echo: PB10
static uint16_t s_dist2 = 0; // 센서 2 (입구 내부) - Trig: PA7, Echo: PB15
static uint16_t s_dist3 = 0; // 센서 3 (출구 내부) - Trig: PA9, Echo: PB14
static uint16_t s_dist4 = 0; // 센서 4 (출구 외부) - Trig: PA8, Echo: PB13

// 주기 제어 타이머 (HAL_GetTick 기반 Non-blocking)
static uint32_t s_last_sensor_tick = 0;
static uint32_t s_last_fsm_tick = 0;
static uint32_t s_last_debug_tick = 0;
static uint8_t  s_sensor_channel = 0;

/**
 * @brief 센서별 거리 측정 함수 (HC-SR04 초음파 센서 드라이버 연동)
 */
static uint16_t Read_Ultrasonic_Sensor(uint8_t sensor_idx)
{
    float dist_cm = 0.0f;
    if (hcSr04Read(&dist_cm, sensor_idx))
    {
        return (uint16_t)dist_cm;
    }
    return 0; // 측정 실패 또는 범위 초과 시 0
}

void apInit(void)
{
    // 초음파 센서(HC-SR04) 4개 및 타이머 초기화
    hcSr04Init();

    // RGB LED(입구 1번, 출구 2번) 초기화 (기본 상태: GREEN)
    RgbLed_Init();

    // 주차 차단기 서보 모터(SG90) 및 FSM 초기화
    SG90_Init();

    printf("\r\n========================================\r\n");
    printf("   Parking Lot System Initialized\r\n");
    printf("   S1:PA6/PB10, S2:PA7/PB15, S3:PA9/PB14, S4:PA8/PB13\r\n");
    printf("   LED1(Gate1): PA0(R)/PA1(G)/PA4(B)\r\n");
    printf("   LED2(Gate2): PB0(R)/PC1(G)/PC0(B)\r\n");
    printf("   HC-SR04 Trigger Interval: %d ms\r\n", SENSOR_TRIGGER_INTERVAL_MS);
    printf("========================================\r\n");
}

void apMain(void)
{
    uint32_t current_tick = HAL_GetTick();

    // 0. RGB LED 점멸/애니메이션 주기 업데이트 (Non-blocking)
    RgbLed_Update(current_tick);

    // 1. 초음파 센서 4개를 60ms 간격으로 1개씩 순환(Round-Robin) 트리거 및 측정
    //    (HAL_Delay 없이 HAL_GetTick 기반 논블로킹으로 잔여 초음파 간섭 방지)
    if (current_tick - s_last_sensor_tick >= SENSOR_TRIGGER_INTERVAL_MS)
    {
        s_last_sensor_tick = current_tick;

        switch (s_sensor_channel)
        {
            case 0:
                s_dist1 = Read_Ultrasonic_Sensor(0);
                break;
            case 1:
                s_dist2 = Read_Ultrasonic_Sensor(1);
                break;
            case 2:
                s_dist3 = Read_Ultrasonic_Sensor(2);
                break;
            case 3:
                s_dist4 = Read_Ultrasonic_Sensor(3);
                break;
        }
        s_sensor_channel = (s_sensor_channel + 1) % 4;
    }

    // 2. 주차 차단기 모터 스무스 이동 및 FSM 제어 (10ms 주기)
    if (current_tick - s_last_fsm_tick >= FSM_PROCESS_PERIOD_MS)
    {
        s_last_fsm_tick = current_tick;
        SG90_ProcessParkingLane(s_dist1, s_dist2, s_dist3, s_dist4, DETECTION_THRESHOLD_CM);
    }

    // 3. UART 시리얼 디버그 출력 (300ms 주기)
    if (current_tick - s_last_debug_tick >= DEBUG_PRINT_PERIOD_MS)
    {
        s_last_debug_tick = current_tick;
        printf("[Sensors] S1:%2dcm | S2:%2dcm | S3:%2dcm | S4:%2dcm | State:%d | Count:%2d | LED1:%d | LED2:%d\r\n",
               s_dist1, s_dist2, s_dist3, s_dist4, SG90_GetLaneState(), SG90_GetLaneCount(),
               RgbLed_GetColor(RGB_LED_1), RgbLed_GetColor(RGB_LED_2));
    }
}