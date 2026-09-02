#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_PARKING_SLOTS   32  /* 필요 시 구역 확장 대비 최대 슬롯 수 */

typedef struct {
    char section;           /* 'A', 'B', 'C' 등 */
    uint8_t number;         /* 1, 2, 3 ... */
    bool is_occupied;       /* true: 주차됨 (1), false: 빈자리 (0) */
    bool is_valid;          /* 슬롯 활성화 여부 */
} ParkingSlotInfo_t;

typedef struct {
    ParkingSlotInfo_t slots[MAX_PARKING_SLOTS];
    uint8_t total_count;
    uint8_t occupied_count;
    uint8_t empty_count;
} ParkingSystem_t;

/* 파서 및 조회 함수 */
void parkingModelInit(void);
bool parkingModelParsePacket(const char *packet_str);
const ParkingSystem_t* parkingModelGetSystem(void);