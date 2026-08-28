#pragma once

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SG90_PULSE_0_DEG      500  // 0도 (500us, 닫힘)
#define SG90_PULSE_90_DEG     1450 // 90도 (1450us, 열림)

/*
 * [속도 프리셋 안내]
 * 1.5f : 약 0.63초 (빠르고 경쾌함)
 * 1.0f : 약 0.95초 (1초 내외 - 묵직하고 자연스러운 밸런스)
 * 0.63f: 약 1.50초 (실제 대형 차단기 느낌의 느리고 리얼한 연출)
 */
#define SG90_SPEED_US_PER_MS  0.63f // 현재: 약 1.5초 설정

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
