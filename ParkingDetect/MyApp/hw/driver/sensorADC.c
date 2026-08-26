// #include "sensorADC.h"
// #include "adc.h"

// #define VREF_VOLT           3.3f
// #define ADC_MAX_VAL         4095.0f

// /* 풀업 회로 기준 임계값 설정 (어두워질수록 값 증가) */
// #define THRESHOLD_OCCUPIED  900    /* 이 값 이상으로 어두워지면 주차 진입 판단 */
// #define THRESHOLD_EMPTY     899    /* 이 값 이하로 밝아지면 출차 판단 */
// #define DEBOUNCE_COUNT      5       /* 연속 5회(약 500ms) 유지 시 상태 확정 */

// typedef struct {
//     ParkingStatus_t status;
//     uint8_t match_count;
// } ParkingSlot_t;

// /* DMA 버퍼 (Half-Word 단위) */
// static uint16_t adc_dma_buf[CDS_CHANNEL_COUNT] = {0};
// static volatile bool is_conv_done = false;

// /* 주차 상태 관리 구조체 */
// static ParkingSlot_t parking_slots[CDS_CHANNEL_COUNT];

// /**
//  * @brief ADC DMA 변환 및 주차 상태 초기화
//  */
// void adcInit(void) {
//     is_conv_done = false;

//     for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
//         parking_slots[i].status = PARKING_EMPTY;
//         parking_slots[i].match_count = 0;
//     }

//     HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, CDS_CHANNEL_COUNT);
// }

// /**
//  * @brief 주기적 FSM 업데이트 및 주차 판단 (메인 루프에서 호출)
//  */
// void adcUpdate(void) {
//     if (!is_conv_done) {
//         return;
//     }
//     is_conv_done = false;

//     /* 6개 채널 주차 상태 판별 (히스테리시스 + 디바운스) */
//     for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
//         uint16_t raw = adc_dma_buf[i];

//         if (parking_slots[i].status == PARKING_EMPTY) {
//             /* 빈 자리 상태 -> 어두워져 진입 감지 */
//             if (raw >= THRESHOLD_OCCUPIED) {
//                 parking_slots[i].match_count++;
//                 if (parking_slots[i].match_count >= DEBOUNCE_COUNT) {
//                     parking_slots[i].status = PARKING_OCCUPIED;
//                     parking_slots[i].match_count = 0;
//                 }
//             } else {
//                 parking_slots[i].match_count = 0;
//             }
//         } else {
//             /* 주차 상태 -> 밝아져 출차 감지 */
//             if (raw <= THRESHOLD_EMPTY) {
//                 parking_slots[i].match_count++;
//                 if (parking_slots[i].match_count >= DEBOUNCE_COUNT) {
//                     parking_slots[i].status = PARKING_EMPTY;
//                     parking_slots[i].match_count = 0;
//                 }
//             } else {
//                 parking_slots[i].match_count = 0;
//             }
//         }
//     }
// }

// uint16_t adcGetCdsRaw(CdsChannel_t ch) {
//     if (ch >= CDS_CHANNEL_COUNT) return 0;
//     return adc_dma_buf[ch];
// }

// float adcGetCdsVolt(CdsChannel_t ch) {
//     if (ch >= CDS_CHANNEL_COUNT) return 0.0f;
//     return ((float)adc_dma_buf[ch] * VREF_VOLT) / ADC_MAX_VAL;
// }

// uint16_t adcGetCdsPA0(void) { return adc_dma_buf[CDS_PA0]; }
// uint16_t adcGetCdsPA1(void) { return adc_dma_buf[CDS_PA1]; }
// uint16_t adcGetCdsPA4(void) { return adc_dma_buf[CDS_PA4]; }
// uint16_t adcGetCdsPB0(void) { return adc_dma_buf[CDS_PB0]; }
// uint16_t adcGetCdsPC0(void) { return adc_dma_buf[CDS_PC0]; }
// uint16_t adcGetCdsPC1(void) { return adc_dma_buf[CDS_PC1]; }

// ParkingStatus_t adcGetParkingStatus(CdsChannel_t ch) {
//     if (ch >= CDS_CHANNEL_COUNT) return PARKING_EMPTY;
//     return parking_slots[ch].status;
// }

// bool adcIsOccupied(CdsChannel_t ch) {
//     return (adcGetParkingStatus(ch) == PARKING_OCCUPIED);
// }

// void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
//     if (hadc->Instance == ADC1) {
//         is_conv_done = true;
//     }
// }

#include "sensorADC.h"
#include "adc.h"

#define VREF_VOLT           3.3f
#define ADC_MAX_VAL         4095.0f

#define THRESHOLD_OCCUPIED  800
#define THRESHOLD_EMPTY     400
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

        if (parking_slots[i].status == PARKING_EMPTY) {
            if (raw >= THRESHOLD_OCCUPIED) {
                parking_slots[i].match_count++;
                if (parking_slots[i].match_count >= DEBOUNCE_COUNT) {
                    parking_slots[i].status = PARKING_OCCUPIED;
                    parking_slots[i].match_count = 0;
                }
            } else {
                parking_slots[i].match_count = 0;
            }
        } else {
            if (raw <= THRESHOLD_EMPTY) {
                parking_slots[i].match_count++;
                if (parking_slots[i].match_count >= DEBOUNCE_COUNT) {
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