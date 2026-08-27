#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define CDS_CHANNEL_COUNT   6

/* CDS 채널 및 주차 슬롯 ID */
typedef enum {
    SLOT_A01 = 0,    /* PA0  - ADC_IN0  */
    SLOT_A02 = 1,    /* PA1  - ADC_IN1  */
    SLOT_A03 = 2,    /* PA4  - ADC_IN4  */
    SLOT_A04 = 3,    /* PB0  - ADC_IN8  */
    SLOT_A05 = 4,    /* PC0  - ADC_IN10 */
    SLOT_A06 = 5     /* PC1  - ADC_IN11 */
} ParkingSlotID_t;

/* 하위 호환용 alias */
typedef ParkingSlotID_t CdsChannel_t;
#define CDS_PA0 SLOT_A01
#define CDS_PA1 SLOT_A02
#define CDS_PA4 SLOT_A03
#define CDS_PB0 SLOT_A04
#define CDS_PC0 SLOT_A05
#define CDS_PC1 SLOT_A06

/* 주차 점유 상태 */
typedef enum {
    PARKING_EMPTY = 0,     /* 비점유 */
    PARKING_OCCUPIED = 1   /* 점유 */
} ParkingStatus_t;

void adcInit(void);
void adcUpdate(void);

/* 센서 및 상태 조회 */
uint16_t adcGetCdsRaw(ParkingSlotID_t slot);
float adcGetCdsVolt(ParkingSlotID_t slot);
ParkingStatus_t adcGetParkingStatus(ParkingSlotID_t slot);
bool adcIsOccupied(ParkingSlotID_t slot);

/* 주차 구역 이름(식별자) 조회 */
const char* parkingGetSlotName(ParkingSlotID_t slot);