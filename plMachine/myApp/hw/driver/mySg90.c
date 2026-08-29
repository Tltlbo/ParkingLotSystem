#include "mySg90.h"
#include "myRgbLed.h"

extern TIM_HandleTypeDef htim4;

static const uint32_t s_channels[SG90_GATE_COUNT] = {
    TIM_CHANNEL_3, // SG90_ENTRY_GATE (PB8)
    TIM_CHANNEL_4  // SG90_EXIT_GATE (PB9)
};

typedef struct {
    float current_pulse;
    float target_pulse;
} SG90_GateControl_t;

static SG90_GateControl_t s_gates[SG90_GATE_COUNT];

static LaneState_t s_lane_state = LANE_STATE_IDLE;
static int8_t s_lane_count = 0; // 통로 내 점유 차량 수 (양수: 입차 중, 음수: 출차 중, 0: 비어있음)
static uint32_t s_lane_timer = 0;
static uint32_t s_clear_timer = 0;
static uint32_t s_last_servo_tick = 0;
static uint32_t s_no_sensor_tick = 0;
static uint8_t s_sensor_cnt[4] = {0, 0, 0, 0};
static uint8_t s_prev_s[4] = {0, 0, 0, 0};

static uint8_t SG90_IsGateMoving(SG90_Gate_t gate)
{
    if (gate >= SG90_GATE_COUNT) return 0;
    return (s_gates[gate].current_pulse != s_gates[gate].target_pulse);
}

static uint8_t Check_Sensor_Debounced(uint8_t sensor_idx, uint16_t dist, uint16_t threshold)
{
    if (dist != 0 && dist <= threshold) {
        if (s_sensor_cnt[sensor_idx] < SG90_DISTANCE_CONFIRM_COUNT) {
            s_sensor_cnt[sensor_idx]++;
        }
    } else {
        s_sensor_cnt[sensor_idx] = 0;
    }
    return (s_sensor_cnt[sensor_idx] >= SG90_DISTANCE_CONFIRM_COUNT);
}

#define BUTTON_ENTRY_PORT   GPIOA
#define BUTTON_ENTRY_PIN    GPIO_PIN_10

static volatile uint8_t s_btn_entry_triggered = 0;
static uint32_t s_last_btn_tick = 0;

/*
 * EXTI 인터럽트 콜백: PA10 버튼을 누르는 순간 즉시 하드웨어 인터럽트로 호출됨
 * 채터링(Bouncing) 방지를 위한 250ms 소프트웨어 디바운스 적용
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BUTTON_ENTRY_PIN)
    {
        uint32_t now = HAL_GetTick();
        if (now - s_last_btn_tick >= 250)
        {
            s_last_btn_tick = now;
            s_btn_entry_triggered = 1;
        }
    }
}

// 경과 시간(dt)에 비례하여 펄스를 부드럽게 이동 (센서 측정 딜레이가 생겨도 완벽한 등속 이동 보장)
static void SG90_UpdateSmoothMovement(uint32_t now)
{
    if (s_last_servo_tick == 0) {
        s_last_servo_tick = now;
        return;
    }

    uint32_t dt_ms = now - s_last_servo_tick;
    if (dt_ms == 0) return;
    s_last_servo_tick = now;

    if (dt_ms > 100) dt_ms = 100; // 비정상 지연 클램핑

    float step = SG90_SPEED_TICKS_PER_MS * (float)dt_ms;

    for (uint8_t i = 0; i < SG90_GATE_COUNT; i++) {
        if (s_gates[i].current_pulse < s_gates[i].target_pulse) {
            s_gates[i].current_pulse += step;
            if (s_gates[i].current_pulse > s_gates[i].target_pulse) {
                s_gates[i].current_pulse = s_gates[i].target_pulse;
            }
            __HAL_TIM_SET_COMPARE(&htim4, s_channels[i], (uint32_t)s_gates[i].current_pulse);
        }
        else if (s_gates[i].current_pulse > s_gates[i].target_pulse) {
            s_gates[i].current_pulse -= step;
            if (s_gates[i].current_pulse < s_gates[i].target_pulse) {
                s_gates[i].current_pulse = s_gates[i].target_pulse;
            }
            __HAL_TIM_SET_COMPARE(&htim4, s_channels[i], (uint32_t)s_gates[i].current_pulse);
        }
    }
}

int8_t SG90_GetLaneCount(void)
{
    return s_lane_count;
}

void SG90_Init(void)
{
    for (uint8_t i = 0; i < SG90_GATE_COUNT; i++) {
        HAL_TIM_PWM_Start(&htim4, s_channels[i]);
        s_gates[i].current_pulse = (float)SG90_PULSE_90_DEG;
        s_gates[i].target_pulse  = (float)SG90_PULSE_90_DEG;
        __HAL_TIM_SET_COMPARE(&htim4, s_channels[i], (uint32_t)SG90_PULSE_90_DEG);
    }
    s_lane_state = LANE_STATE_IDLE;
    s_lane_count = 0;
    s_lane_timer = 0;
    s_clear_timer = 0;
    s_last_servo_tick = 0;
    s_no_sensor_tick = 0;
    for (uint8_t i = 0; i < 4; i++) {
        s_sensor_cnt[i] = 0;
        s_prev_s[i] = 0;
    }

    // PA10 버튼 핀 EXTI 인터럽트 초기화 (Falling Edge, Pull-Up)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = BUTTON_ENTRY_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BUTTON_ENTRY_PORT, &GPIO_InitStruct);

    /* EXTI15_10 인터럽트 우선순위 설정 및 활성화 */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    s_btn_entry_triggered = 0;
    s_last_btn_tick = 0;

    RgbLed_SetColor(RGB_LED_1, LED_COLOR_GREEN);
    RgbLed_SetColor(RGB_LED_2, LED_COLOR_GREEN);
}

void SG90_SetGateAngle(SG90_Gate_t gate, uint8_t angle)
{
    if (gate >= SG90_GATE_COUNT) return;

    if (angle == GATE_ANGLE_OPEN || angle == 0) {
        s_gates[gate].target_pulse = (float)SG90_PULSE_0_DEG;   // 0도 열림 (50 ticks)
    } else {
        s_gates[gate].target_pulse = (float)SG90_PULSE_90_DEG;  // 90도 닫힘 (150 ticks, 초기값)
    }
}

LaneState_t SG90_GetLaneState(void)
{
    return s_lane_state;
}

void SG90_ProcessParkingLane(uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, uint16_t threshold_cm)
{
    uint32_t now = HAL_GetTick();

    // 1. 서보 모터 스무스 이동 업데이트 (시간 기반)
    SG90_UpdateSmoothMovement(now);

    uint8_t s1_active = Check_Sensor_Debounced(0, d1, threshold_cm);
    uint8_t s2_active = Check_Sensor_Debounced(1, d2, threshold_cm);
    uint8_t s3_active = Check_Sensor_Debounced(2, d3, threshold_cm);
    uint8_t s4_active = Check_Sensor_Debounced(3, d4, threshold_cm);

    // 센서 Edge(상승/하강 엣지) 판별
    uint8_t s1_rising  = (s1_active && !s_prev_s[0]);
    uint8_t s1_falling = (!s1_active && s_prev_s[0]);
    uint8_t s2_rising  = (s2_active && !s_prev_s[1]);
    uint8_t s3_rising  = (s3_active && !s_prev_s[2]);
    uint8_t s4_rising  = (s4_active && !s_prev_s[3]);
    uint8_t s4_falling = (!s4_active && s_prev_s[3]);

    // 2. PA10 관리자 비상 개폐 스위치 인터럽트 처리 (사용자 지정 3가지 상태별 로직)
    if (s_btn_entry_triggered)
    {
        s_btn_entry_triggered = 0; // 플래그 클리어

        // ---------------------------------------------------------------------
        // [상황 3] 입차로 적색, 출차로 녹색 (출차 진행 중: s_lane_count < 0)
        // ---------------------------------------------------------------------
        if (s_lane_count < 0)
        {
            if (s_gates[SG90_ENTRY_GATE].target_pulse == (float)SG90_PULSE_0_DEG)
            {
                /* 센서 1 미감지 등으로 차는 나갔는데 안 닫힐 때 -> 버튼 누르면 모터 닫고 그때 카운트 증가(+1) */
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_CLOSE);
                s_lane_count++; // ⭐ S1 미감지 시 수동 카운트 증가(0 회복)
                s_clear_timer = 0;
            }
            else
            {
                /* 차가 안에서 나오는데 S2 미감지로 문이 안 열릴 때 -> 버튼 누르면 모터 열림, 카운트 증감 없음 */
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_OPEN);
                s_lane_state = LANE_STATE_REV_ENTRY_OPEN;
                s_lane_timer = now;
            }
        }
        // ---------------------------------------------------------------------
        // [상황 2] 입차로 녹색, 출차로 적색 (입차 진행 중: s_lane_count > 0)
        // ---------------------------------------------------------------------
        else if (s_lane_count > 0)
        {
            if (s_gates[SG90_ENTRY_GATE].target_pulse == (float)SG90_PULSE_0_DEG)
            {
                /* 센서 2 미감지로 차는 들어갔는데 안 닫힐 때 -> 버튼 누르면 모터 닫힘, 카운트 변화 없음 */
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_CLOSE);
            }
            else
            {
                /* 닫혀있을 때 뒤차 수동 진입 -> 버튼 누르면 모터 열림 & 카운트 1 증가 */
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_OPEN);
                if (s_lane_count < 5) {
                    s_lane_count++;
                }
                s_lane_timer = now;
            }
        }
        // ---------------------------------------------------------------------
        // [상황 1] 둘 다 녹색 (대기 IDLE: s_lane_count == 0)
        // ---------------------------------------------------------------------
        else
        {
            if (s_gates[SG90_ENTRY_GATE].target_pulse == (float)SG90_PULSE_0_DEG)
            {
                /* 센서 2 미감지로 차는 들어갔는데 안 닫힐 때 -> 버튼 누르면 모터 닫힘, 카운트 변화 없음 */
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_CLOSE);
            }
            else
            {
                /* 둘 다 녹색일 때 버튼 누르면 -> 모터 열림 & 카운트 1 증가 (0 -> 1) */
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_OPEN);
                s_lane_count = 1;
                s_lane_state = LANE_STATE_ENTRY_OPEN;
                s_lane_timer = now;
            }
        }
    }

    // 3. 다중 차량 및 방향별 FSM 상태 머신 제어
    switch (s_lane_state)
    {
        case LANE_STATE_IDLE:
            RgbLed_SetColor(RGB_LED_1, LED_COLOR_GREEN);
            RgbLed_SetColor(RGB_LED_2, LED_COLOR_GREEN);
            s_lane_count = 0;

            if (s1_active) {
                /* 정방향 입차 시작 (Count = 1) */
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_OPEN);
                s_lane_state = LANE_STATE_ENTRY_OPEN;
                s_lane_timer = now;
                s_lane_count = 1;
            }
            else if (s4_active) {
                /* 역방향 출차 시작 (Count = -1) */
                SG90_SetGateAngle(SG90_EXIT_GATE, GATE_ANGLE_OPEN);
                s_lane_state = LANE_STATE_REV_EXIT_OPEN;
                s_lane_timer = now;
                s_lane_count = -1;
            }
            break;

        /* ==================== 정방향: 다중 입차 시퀀스 ==================== */
        case LANE_STATE_ENTRY_OPEN:
        case LANE_STATE_ENTRY_CLOSE:
        case LANE_STATE_EXIT_OPEN:
        case LANE_STATE_EXIT_CLOSE:
            /* 신호등: 입차로는 녹색(GREEN) 고정, 출차로는 진입 금지 경광등 */
            RgbLed_SetColor(RGB_LED_1, LED_COLOR_GREEN);
            RgbLed_SetColor(RGB_LED_2, LED_COLOR_BLINK_RED_YELLOW);

            /* [입차 차단기 1번 제어 및 S1 감지 시 카운트 증가] */
            if (s1_rising) {
                // 새로운 뒤차가 S1에 도착하여 감지되는 순간 카운트 1 증가 & 차단기 열림
                if (s_lane_count < 5) {
                    s_lane_count++;
                }
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_OPEN);
                s_lane_timer = now;
            } else if (s1_active) {
                // S1에 차량이 머무는 동안은 열림(0도) 유지
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_OPEN);
            }

            /* [센서 2번 감지 시 모터 0 -> 90도 닫힘] S1에 뒤차가 없고 S2에 차가 통과했으면 차단기 닫힘 */
            if (!s1_active && (now - s_lane_timer >= SG90_MIN_OPEN_HOLD_MS) && s2_active) {
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_CLOSE);
            }

            /* [출차 차단기 2번 제어] 통로 내 차량이 S3에 도달하면 출차 차단기 0도 개방 */
            if (s3_active) {
                SG90_SetGateAngle(SG90_EXIT_GATE, GATE_ANGLE_OPEN);
            }

            /* [출차 차단기 닫힘 조건] 차가 S4를 통과하고 있고 S3에 바로 다음 차가 없으면 닫기 */
            if (!s3_active && (now - s_lane_timer >= SG90_MIN_OPEN_HOLD_MS) && s4_active) {
                if (s_gates[SG90_EXIT_GATE].target_pulse == (float)SG90_PULSE_0_DEG) {
                    SG90_SetGateAngle(SG90_EXIT_GATE, GATE_ANGLE_CLOSE);
                }
            }

            /* [출구 이탈 카운트 감소] 차가 S4를 빠져나갈 때마다 통로 내 차량 수 1 감소 */
            if (s4_falling && s_lane_count > 0) {
                s_lane_count--;
                s_clear_timer = 0;
            }

            /* [IDLE 복귀 조건] 통로 내 모든 차량 출차 완료 + 입차 차단기 닫힘 + 센서 비어있을 때 */
            if (s_lane_count == 0 && !s1_active && !s4_active &&
                s_gates[SG90_ENTRY_GATE].target_pulse == (float)SG90_PULSE_90_DEG &&
                !SG90_IsGateMoving(SG90_EXIT_GATE))
            {
                if (s_clear_timer == 0) {
                    s_clear_timer = now;
                } else if (now - s_clear_timer >= 1000) {
                    // 모든 차 완전 이탈 후 1초 안정화 -> IDLE 복귀
                    s_lane_state = LANE_STATE_IDLE;
                    s_clear_timer = 0;
                    RgbLed_SetColor(RGB_LED_1, LED_COLOR_GREEN);
                    RgbLed_SetColor(RGB_LED_2, LED_COLOR_GREEN);
                }
            } else {
                s_clear_timer = 0;
            }
            break;

        /* ==================== 역방향: 다중 출차 시퀀스 ==================== */
        case LANE_STATE_REV_EXIT_OPEN:
        case LANE_STATE_REV_EXIT_CLOSE:
        case LANE_STATE_REV_ENTRY_OPEN:
        case LANE_STATE_REV_ENTRY_CLOSE:
            /* 신호등: 출차로는 녹색(GREEN) 고정, 입차로는 진입 금지 경광등 */
            RgbLed_SetColor(RGB_LED_2, LED_COLOR_GREEN);
            RgbLed_SetColor(RGB_LED_1, LED_COLOR_BLINK_RED_YELLOW);

            /* [출차 차단기 2번 제어 및 S4 감지 시 카운트 증가(음수)] */
            if (s4_rising) {
                // 새로운 뒤차가 S4에 도착하여 감지되는 순간 음수 카운트 1 감소 & 차단기 열림
                if (s_lane_count > -5) {
                    s_lane_count--;
                }
                SG90_SetGateAngle(SG90_EXIT_GATE, GATE_ANGLE_OPEN);
                s_lane_timer = now;
            } else if (s4_active) {
                // S4에 차량이 머무는 동안은 열림(0도) 유지
                SG90_SetGateAngle(SG90_EXIT_GATE, GATE_ANGLE_OPEN);
            }

            /* [센서 3번 감지 시 모터 닫힘] S4에 차가 없고 S3에 차가 통과했으면 출차 차단기 닫힘 */
            if (!s4_active && (now - s_lane_timer >= SG90_MIN_OPEN_HOLD_MS) && s3_active) {
                SG90_SetGateAngle(SG90_EXIT_GATE, GATE_ANGLE_CLOSE);
            }

            /* [입차 차단기 1번 제어] 통로 내 차량이 S2에 도달하면 입차 차단기 0도 개방 */
            if (s2_active) {
                SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_OPEN);
            }

            /* [입차 차단기 닫힘 조건] 차가 S1을 통과하고 있고 S2에 바로 다음 차가 없으면 닫기 */
            if (!s2_active && (now - s_lane_timer >= SG90_MIN_OPEN_HOLD_MS) && s1_active) {
                if (s_gates[SG90_ENTRY_GATE].target_pulse == (float)SG90_PULSE_0_DEG) {
                    SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_CLOSE);
                }
            }

            /* [뒤쪽 센서 S1 감지 후 이탈 시 카운트 증가] 차가 S1을 빠져나갈 때마다 음수 카운트 1씩 회복 (+1) */
            if (s1_falling && s_lane_count < 0) {
                s_lane_count++;
                s_clear_timer = 0;
            }

            /* [IDLE 복귀 조건] 통로 내 모든 출차 차량 완료 + 출차 차단기 닫힘 + 센서 비어있을 때 */
            if (s_lane_count == 0 && !s1_active && !s4_active &&
                s_gates[SG90_EXIT_GATE].target_pulse == (float)SG90_PULSE_90_DEG &&
                !SG90_IsGateMoving(SG90_ENTRY_GATE))
            {
                if (s_clear_timer == 0) {
                    s_clear_timer = now;
                } else if (now - s_clear_timer >= 1000) {
                    // 모든 차 완전 이탈 후 1초 안정화 -> IDLE 복귀
                    s_lane_state = LANE_STATE_IDLE;
                    s_clear_timer = 0;
                    RgbLed_SetColor(RGB_LED_1, LED_COLOR_GREEN);
                    RgbLed_SetColor(RGB_LED_2, LED_COLOR_GREEN);
                }
            } else {
                s_clear_timer = 0;
            }
            break;
    }

    // 4. 안전 장치: Watchdog Auto-Zero (모든 센서 15초간 무감지 시 영점 자동 리셋)
    if (!s1_active && !s2_active && !s3_active && !s4_active &&
        !SG90_IsGateMoving(SG90_ENTRY_GATE) && !SG90_IsGateMoving(SG90_EXIT_GATE))
    {
        if (s_no_sensor_tick == 0) {
            s_no_sensor_tick = now;
        } else if (now - s_no_sensor_tick >= SG90_LANE_TIMEOUT_MS) {
            // 15초간 통로 내 움직임이 없으면 강제 영점 및 IDLE 복구
            s_lane_count = 0;
            s_lane_state = LANE_STATE_IDLE;
            SG90_SetGateAngle(SG90_ENTRY_GATE, GATE_ANGLE_CLOSE);
            SG90_SetGateAngle(SG90_EXIT_GATE, GATE_ANGLE_CLOSE);
            RgbLed_SetColor(RGB_LED_1, LED_COLOR_GREEN);
            RgbLed_SetColor(RGB_LED_2, LED_COLOR_GREEN);
            s_no_sensor_tick = 0;
        }
    } else {
        s_no_sensor_tick = 0;
    }

    // 이전 센서 상태 업데이트
    s_prev_s[0] = s1_active;
    s_prev_s[1] = s2_active;
    s_prev_s[2] = s3_active;
    s_prev_s[3] = s4_active;
}