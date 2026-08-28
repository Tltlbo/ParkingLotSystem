#pragma once

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

/*
 * TIM4 클럭: 84MHz / 840 = 100kHz (1 tick = 10us)
 * Period (ARR) = 2000 - 1 (20ms, 50Hz)
 * 0도 (0.5ms = 500us)   : 50 ticks
 * 90도 (1.45ms = 1450us): 145 ticks
 */
#define SG90_PULSE_0_DEG      50   // 0도 (500us, 닫힘)
#define SG90_PULSE_90_DEG     145  // 90도 (1450us, 열림)

#define SG90_SPEED_TICKS_PER_MS  0.1f  // 1ms당 0.1 tick (95 ticks 이동에 약 1.0초 소요)

#define SG90_DISTANCE_CONFIRM_COUNT  2     // 순환 측정 기준 2회 연속 감지 시 확정
#define SG90_MIN_OPEN_HOLD_MS        1500  // 차단기 열림 완료 보장 및 최소 열림 유지 시간 (1.5초)
#define SG90_LANE_TIMEOUT_MS         15000 // 타임아웃 복구 시간 (15초)

typedef enum {
    SG90_ENTRY_GATE = 0, // PB8 (입차 차단기)
    SG90_EXIT_GATE,      // PB9 (출차 차단기)
    SG90_GATE_COUNT
} SG90_Gate_t;

typedef enum {
    LANE_STATE_IDLE = 0,
    /* [정방향: 입차 시퀀스 (센서1 -> 센서2 -> 센서3 -> 센서4)] */
    LANE_STATE_ENTRY_OPEN,
    LANE_STATE_ENTRY_CLOSE,
    LANE_STATE_EXIT_OPEN,
    LANE_STATE_EXIT_CLOSE,
    /* [역방향: 출차 시퀀스 (센서4 -> 센서3 -> 센서2 -> 센서1)] */
    LANE_STATE_REV_EXIT_OPEN,
    LANE_STATE_REV_EXIT_CLOSE,
    LANE_STATE_REV_ENTRY_OPEN,
    LANE_STATE_REV_ENTRY_CLOSE
} LaneState_t;

void SG90_Init(void);
void SG90_SetGateAngle(SG90_Gate_t gate, uint8_t angle);
void SG90_ProcessParkingLane(uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, uint16_t threshold_cm);
LaneState_t SG90_GetLaneState(void);
