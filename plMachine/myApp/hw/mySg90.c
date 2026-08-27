#include "mySg90.h"

extern TIM_HandleTypeDef htim4;

static const uint32_t s_channels[SG90_GATE_COUNT] = {
    TIM_CHANNEL_3, TIM_CHANNEL_4
};

static uint8_t s_sensor4_locked = 0;
static LaneState_t s_lane_state = LANE_STATE_IDLE;
static uint32_t s_lane_timer = 0;
static uint8_t s_sensor_cnt[4] = {0, 0, 0, 0};

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

void SG90_Init(void)
{
    for (uint8_t i = 0; i < SG90_GATE_COUNT; i++) {
        HAL_TIM_PWM_Start(&htim4, s_channels[i]);
        __HAL_TIM_SET_COMPARE(&htim4, s_channels[i], SG90_PULSE_0_DEG);
    }
    s_sensor4_locked = 0;
    s_lane_state = LANE_STATE_IDLE;
}

void SG90_SetGateAngle(SG90_Gate_t gate, uint8_t angle)
{
    if (gate >= SG90_GATE_COUNT || (angle != 0 && angle != 90)) return;

    __HAL_TIM_SET_COMPARE(&htim4, s_channels[gate],
        (angle == 90) ? SG90_PULSE_90_DEG : SG90_PULSE_0_DEG);
}

void SG90_SetSensor4Locked(uint8_t locked)
{
    s_sensor4_locked = (locked != 0);
}

uint8_t SG90_IsSensor4Locked(void)
{
    return s_sensor4_locked;
}

void SG90_ProcessParkingLane(uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, uint16_t threshold_cm)
{
    uint8_t s1_active = Check_Sensor_Debounced(0, d1, threshold_cm);
    uint8_t s2_active = Check_Sensor_Debounced(1, d2, threshold_cm);
    uint8_t s3_active = Check_Sensor_Debounced(2, d3, threshold_cm);
    uint8_t s4_active = Check_Sensor_Debounced(3, d4, threshold_cm);

    uint32_t now = HAL_GetTick();

    switch (s_lane_state)
    {
        case LANE_STATE_IDLE:
            if (s1_active) {
                SG90_SetGateAngle(SG90_ENTRY_GATE, 90); // PB8 -> 90도
                SG90_SetSensor4Locked(1);                // 센서 4 잠금
                s_lane_state = LANE_STATE_ENTRY_OPEN;
                s_lane_timer = now;
            }
            break;

        case LANE_STATE_ENTRY_OPEN:
            if (s2_active) {
                SG90_SetGateAngle(SG90_ENTRY_GATE, 0);  // PB8 -> 0도
                s_lane_state = LANE_STATE_ENTRY_CLOSE;
                s_lane_timer = now;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                SG90_SetGateAngle(SG90_ENTRY_GATE, 0);
                SG90_SetSensor4Locked(0);
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_ENTRY_CLOSE:
            if (s3_active) {
                SG90_SetGateAngle(SG90_EXIT_GATE, 90);  // PB9 -> 90도
                SG90_SetSensor4Locked(0);               // 센서 4 잠금 해제
                s_lane_state = LANE_STATE_EXIT_OPEN;
                s_lane_timer = now;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                SG90_SetSensor4Locked(0);
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_EXIT_OPEN:
            // 센서 4 잠금이 풀려있고 감지된 경우에만 닫힘 전이
            if (!s_sensor4_locked && s4_active) {
                SG90_SetGateAngle(SG90_EXIT_GATE, 0);   // PB9 -> 0도
                s_lane_state = LANE_STATE_EXIT_CLOSE;
                s_lane_timer = now;
            }
            else if (now - s_lane_timer > SG90_LANE_TIMEOUT_MS) {
                SG90_SetGateAngle(SG90_EXIT_GATE, 0);
                s_lane_state = LANE_STATE_IDLE;
            }
            break;

        case LANE_STATE_EXIT_CLOSE:
            // 모든 센서가 클리어되거나 짧은 지연 후 IDLE 복귀
            if (!s4_active) {
                s_lane_state = LANE_STATE_IDLE;
            }
            else if (now - s_lane_timer > 3000) {
                s_lane_state = LANE_STATE_IDLE;
            }
            break;
    }
}