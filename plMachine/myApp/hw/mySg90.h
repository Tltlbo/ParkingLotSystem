#pragma once

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SG90_PULSE_0_DEG      50   // 0도 (닫힘)
#define SG90_PULSE_90_DEG     145  // 90도 (열림)

#define SG90_DISTANCE_CONFIRM_COUNT  5     // 10ms 호출 기준 50ms 확정
#define SG90_LANE_TIMEOUT_MS         15000 // 타임아웃 복구 시간 (15초)

typedef enum {
    SG90_ENTRY_GATE = 0, // PB8 (입차)
    SG90_EXIT_GATE,      // PB9 (출차)
    SG90_GATE_COUNT
} SG90_Gate_t;

typedef enum {
    LANE_STATE_IDLE = 0,
    LANE_STATE_ENTRY_OPEN,
    LANE_STATE_ENTRY_CLOSE,
    LANE_STATE_EXIT_OPEN,
    LANE_STATE_EXIT_CLOSE
} LaneState_t;

void SG90_Init(void);
void SG90_SetGateAngle(SG90_Gate_t gate, uint8_t angle);
void SG90_SetSensor4Locked(uint8_t locked);
uint8_t SG90_IsSensor4Locked(void);

void SG90_ProcessParkingLane(uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, uint16_t threshold_cm);
LaneState_t SG90_GetLaneState(void);