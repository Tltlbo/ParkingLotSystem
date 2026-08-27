#include "sensorADC.h"
#include "adc.h"

#define VREF_VOLT           3.3f
#define ADC_MAX_VAL         4095.0f

#define THRESHOLD_OCCUPIED  800
#define THRESHOLD_EMPTY     650
#define DEBOUNCE_COUNT      5

/* 슬롯 네이밍 테이블 (필요시 원하는 이름으로 변경 가능) */
static const char *SLOT_NAMES[CDS_CHANNEL_COUNT] = {
    "A-01", /* PA0 */
    "A-02", /* PA1 */
    "A-03", /* PA4 */
    "A-04", /* PB0 */
    "A-05", /* PC0 */
    "A-06"  /* PC1 */
};

typedef struct {
    ParkingStatus_t status;
    uint8_t match_count;
} ParkingSlot_t;

static uint16_t adc_dma_buf[CDS_CHANNEL_COUNT] = {0};
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

        /* 1. 현재 빈 자리(EMPTY)일 때 -> 진입(OCCUPIED) 감지 */
        if (parking_slots[i].status == PARKING_EMPTY) {
            if (raw >= THRESHOLD_OCCUPIED) {
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
            if (raw <= THRESHOLD_EMPTY) {
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