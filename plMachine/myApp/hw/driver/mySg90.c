#include "mySg90.h"

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
static uint32_t s_lane_timer = 0;
static uint32_t s_clear_timer = 0;
static uint8_t s_sensor_cnt[4] = {0, 0, 0, 0};

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

static uint32_t s_last_servo_tick = 0;

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

    float step = SG90_SPEED_US_PER_MS * (float)dt_ms;

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

void SG90_Init(void)
{
    for (uint8_t i = 0; i < SG90_GATE_COUNT; i++) {
        HAL_TIM_PWM_Start(&htim4, s_channels[i]);
        s_gates[i].current_pulse = (float)SG90_PULSE_0_DEG;
        s_gates[i].target_pulse  = (float)SG90_PULSE_0_DEG;
        __HAL_TIM_SET_COMPARE(&htim4, s_channels[i], (uint32_t)SG90_PULSE_0_DEG);
    }
    s_lane_state = LANE_STATE_IDLE;
    s_lane_timer = 0;
    s_clear_timer = 0;
    s_last_servo_tick = 0;
    for (uint8_t i = 0; i < 4; i++) {
        s_sensor_cnt[i] = 0;
    }
}

void SG90_SetGateAngle(SG90_Gate_t gate, uint8_t angle)
{
    if (gate >= SG90_GATE_COUNT || (angle != 0 && angle != 90)) return;

    s_gates[gate].target_pulse = (angle == 90) ? (float)SG90_PULSE_90_DEG : (float)SG90_PULSE_0_DEG;
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

    switch (s_lane_state)
    {
        case LANE_STATE_IDLE:
            if (s1_active) {
                /* 1. 입차 시작: 센서 1 감지 -> 입차 차단기(PB8) 90도 열림 */
                SG90_SetGateAngle(SG90_ENTRY_GATE, 90);
                s_lane_state = LANE_STATE_ENTRY_OPEN;
                s_lane_timer = now;
            }
            else if (s4_active) {
                /* 2. 출차 시작 (반대 방향): 센서 4 감지 -> 출차 차단기(PB9) 90도 열림 */
                SG90_SetGateAngle(SG90_EXIT_GATE, 90);
                s_lane_state = LANE_STATE_REV_EXIT_OPEN;
                s_lane_timer = now;
            }
            break;

        /* ==================== 정방향: 입차 시퀀스 ==================== */
        case LANE_STATE_ENTRY_OPEN:
            /* 입차 차단기가 완전히 열리고(최소 1.5초 유지), 센서 2 감지 시 닫힘 */
            if ((now - s_lane_timer >= SG90_MIN_OPEN_HOLD_MS) && !SG90_IsGateMoving(SG90_ENTRY_GATE) && s2_active) {
                /* 센서 2 감지 -> 입차 차단기(PB8) 0도 닫힘 */
                SG90_SetGateAngle(SG90_ENTRY_GATE, 0);
                s_lane_state = LANE_STATE_ENTRY_CLOSE;
                s_lane_timer = now;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                SG90_SetGateAngle(SG90_ENTRY_GATE, 0);
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_ENTRY_CLOSE:
            if (s3_active) {
                /* 센서 3 감지 -> 출차 차단기(PB9) 90도 열림 */
                SG90_SetGateAngle(SG90_EXIT_GATE, 90);
                s_lane_state = LANE_STATE_EXIT_OPEN;
                s_lane_timer = now;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_EXIT_OPEN:
            /* 출차 차단기가 완전히 열리고(최소 1.5초 유지), 센서 4 감지 시 닫힘 */
            if ((now - s_lane_timer >= SG90_MIN_OPEN_HOLD_MS) && !SG90_IsGateMoving(SG90_EXIT_GATE) && s4_active) {
                /* 센서 4 감지 -> 출차 차단기(PB9) 0도 닫힘 */
                SG90_SetGateAngle(SG90_EXIT_GATE, 0);
                s_lane_state = LANE_STATE_EXIT_CLOSE;
                s_lane_timer = now;
                s_clear_timer = 0;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                SG90_SetGateAngle(SG90_EXIT_GATE, 0);
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_EXIT_CLOSE:
            /* 차량이 완전히 나갈 때까지 차단기를 0도(닫힘)로 유지하며 대기 */
            /* 차가 센서 4에 머물러 있는 동안에는 절대 IDLE로 가지 않음 (다시 열림 방지) */
            if (!s4_active && !SG90_IsGateMoving(SG90_EXIT_GATE)) {
                if (s_clear_timer == 0) {
                    s_clear_timer = now;
                } else if (now - s_clear_timer >= 1000) {
                    /* 센서 4 완전 이탈 후 1초간 빈 상태 확인 -> 비로소 다음 진입 대기(IDLE) */
                    s_lane_state = LANE_STATE_IDLE;
                    s_clear_timer = 0;
                }
            } else {
                s_clear_timer = 0; // 차가 센서 4에 머물고 있으면 계속 닫힘 대기 유지
            }
            break;

        /* ==================== 역방향: 출차 시퀀스 ==================== */
        case LANE_STATE_REV_EXIT_OPEN:
            /* 출차 차단기가 완전히 열리고(최소 1.5초 유지), 센서 3 감지 시 닫힘 */
            if ((now - s_lane_timer >= SG90_MIN_OPEN_HOLD_MS) && !SG90_IsGateMoving(SG90_EXIT_GATE) && s3_active) {
                /* 센서 3 감지 -> 출차 차단기(PB9) 0도 닫힘 */
                SG90_SetGateAngle(SG90_EXIT_GATE, 0);
                s_lane_state = LANE_STATE_REV_EXIT_CLOSE;
                s_lane_timer = now;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                SG90_SetGateAngle(SG90_EXIT_GATE, 0);
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_REV_EXIT_CLOSE:
            if (s2_active) {
                /* 센서 2 감지 -> 입차 차단기(PB8) 90도 열림 */
                SG90_SetGateAngle(SG90_ENTRY_GATE, 90);
                s_lane_state = LANE_STATE_REV_ENTRY_OPEN;
                s_lane_timer = now;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_REV_ENTRY_OPEN:
            /* 입차 차단기가 완전히 열리고(최소 1.5초 유지), 센서 1 감지 시 닫힘 */
            if ((now - s_lane_timer >= SG90_MIN_OPEN_HOLD_MS) && !SG90_IsGateMoving(SG90_ENTRY_GATE) && s1_active) {
                /* 센서 1 감지 -> 입차 차단기(PB8) 0도 닫힘 */
                SG90_SetGateAngle(SG90_ENTRY_GATE, 0);
                s_lane_state = LANE_STATE_REV_ENTRY_CLOSE;
                s_lane_timer = now;
                s_clear_timer = 0;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                SG90_SetGateAngle(SG90_ENTRY_GATE, 0);
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_REV_ENTRY_CLOSE:
            /* 출차 차량이 센서 1을 완전히 빠져나갈 때까지 차단기를 0도(닫힘)로 유지하며 대기 */
            /* 출차 후 센서 1 앞에 차가 멈춰 있어도 절대 다시 열리지 않음 */
            if (!s1_active && !SG90_IsGateMoving(SG90_ENTRY_GATE)) {
                if (s_clear_timer == 0) {
                    s_clear_timer = now;
                } else if (now - s_clear_timer >= 1000) {
                    /* 센서 1 완전 이탈 후 1초간 빈 상태 확인 -> 비로소 다음 진입 대기(IDLE) */
                    s_lane_state = LANE_STATE_IDLE;
                    s_clear_timer = 0;
                }
            } else {
                s_clear_timer = 0; // 차가 센서 1에 머물고 있으면 계속 닫힘 대기 유지
            }
            break;
    }
}