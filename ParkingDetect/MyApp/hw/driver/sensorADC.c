#include "sensorADC.h"
#include "adc.h"

#define VREF_VOLT           3.3f
#define ADC_MAX_VAL         4095.0f

typedef struct {
    uint16_t thresh_occupied; /* 이 값 이상이면 점유 (OCCUPIED) */
    uint16_t thresh_empty;    /* 이 값 이하이면 비점유 (EMPTY) */
} SlotThreshold_t;

/* 각 슬롯별 임계값 설정 (밝을 때와 어두울 때의 중간값 기준, 히스테리시스 적용) */
static const SlotThreshold_t SLOT_THRESHOLDS[CDS_CHANNEL_COUNT] = {
    [SLOT_A01] = { .thresh_occupied = 1600, .thresh_empty = 1450 }, /* PA0 */
    [SLOT_A02] = { .thresh_occupied = 1600, .thresh_empty = 1450 }, /* PA1 */
    [SLOT_A03] = { .thresh_occupied = 1450, .thresh_empty = 1300 }, /* PA4 */
    [SLOT_A04] = { .thresh_occupied = 1450, .thresh_empty = 1300 }, /* PB0 */
    [SLOT_A05] = { .thresh_occupied = 1300, .thresh_empty = 1180 }, /* PC0 */
    [SLOT_A06] = { .thresh_occupied = 1350, .thresh_empty = 1220 }, /* PC1 */
    [SLOT_B01] = { .thresh_occupied = 3000, .thresh_empty = 2600 }, /* PA6: 밝을 때 2340, 어두울 때 3597 */
    [SLOT_B02] = { .thresh_occupied = 2350, .thresh_empty = 2100 }, /* PA7: 밝을 때 1996, 어두울 때 2625 */
    [SLOT_C01] = { .thresh_occupied = 1800, .thresh_empty = 1650 }, /* PC2 */
    [SLOT_C02] = { .thresh_occupied = 1750, .thresh_empty = 1600 }  /* PC4 */
};

#define DEBOUNCE_COUNT      5

/* 슬롯 네이밍 테이블 (필요시 원하는 이름으로 변경 가능) */
static const char *SLOT_NAMES[CDS_CHANNEL_COUNT] = {
    "A-01", /* PA0 */
    "A-02", /* PA1 */
    "A-03", /* PA4 */
    "A-04", /* PB0 */
    "A-05", /* PC0 */
    "A-06", /* PC1 */
    "B-01", /* PA6 */
    "B-02", /* PA7 */
    "C-01", /* PC2 */
    "C-02"  /* PC4 */
};

typedef struct {
    ParkingStatus_t status;
    uint8_t match_count;
} ParkingSlot_t;

uint16_t adc_dma_buf[CDS_CHANNEL_COUNT] = {0};
static volatile bool is_conv_done = false;
static ParkingSlot_t parking_slots[CDS_CHANNEL_COUNT];

void adcInit(void) {
    is_conv_done = false;
    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        parking_slots[i].status = PARKING_EMPTY;
        parking_slots[i].match_count = 0;
    }
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, CDS_CHANNEL_COUNT);
}

void adcUpdate(void) {
    if (!is_conv_done) return;
    is_conv_done = false;

    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        uint16_t raw = adc_dma_buf[i];
        uint16_t th_occ = SLOT_THRESHOLDS[i].thresh_occupied;
        uint16_t th_emp = SLOT_THRESHOLDS[i].thresh_empty;

        /* 1. 현재 빈 자리(EMPTY)일 때 -> 진입(OCCUPIED) 감지 */
        if (parking_slots[i].status == PARKING_EMPTY) {
            if (raw >= th_occ) {
                if (++parking_slots[i].match_count >= DEBOUNCE_COUNT) {
                    parking_slots[i].status = PARKING_OCCUPIED;
                    parking_slots[i].match_count = 0;
                }
            } else {
                parking_slots[i].match_count = 0;
            }
        }
        /* 2. 현재 주차 중(OCCUPIED)일 때 -> 출차(EMPTY) 감지 */
        else {
            if (raw <= th_emp) {
                if (++parking_slots[i].match_count >= DEBOUNCE_COUNT) {
                    parking_slots[i].status = PARKING_EMPTY;
                    parking_slots[i].match_count = 0;
                }
            } else {
                parking_slots[i].match_count = 0;
            }
        }
    }
}

uint16_t adcGetCdsRaw(ParkingSlotID_t slot) {
    if (slot >= CDS_CHANNEL_COUNT) return 0;
    return adc_dma_buf[slot];
}

float adcGetCdsVolt(ParkingSlotID_t slot) {
    if (slot >= CDS_CHANNEL_COUNT) return 0.0f;
    return ((float)adc_dma_buf[slot] * VREF_VOLT) / ADC_MAX_VAL;
}

ParkingStatus_t adcGetParkingStatus(ParkingSlotID_t slot) {
    if (slot >= CDS_CHANNEL_COUNT) return PARKING_EMPTY;
    return parking_slots[slot].status;
}

bool adcIsOccupied(ParkingSlotID_t slot) {
    return (adcGetParkingStatus(slot) == PARKING_OCCUPIED);
}

const char* parkingGetSlotName(ParkingSlotID_t slot) {
    if (slot >= CDS_CHANNEL_COUNT) return "UNKNOWN";
    return SLOT_NAMES[slot];
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        is_conv_done = true;
    }
}