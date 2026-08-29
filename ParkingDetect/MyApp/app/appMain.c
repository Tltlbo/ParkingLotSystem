#include "appMain.h"
#include "audioPass.h"
#include "sensorADC.h"
#include "UARTTX.h"
#include <stdio.h>

extern int16_t mic_dma_buf[AUDIO_BUF_SIZE * 2];
extern uint16_t adc_dma_buf[CDS_CHANNEL_COUNT];

void appInit(void) {
    adcInit();
    printf("step1: adc done\r\n");

    commUartInit();
    printf("step2: uart done\r\n");

    /* audioInit() 하나로 타이머, PWM, I2S DMA 시작 */
    audioInit();
    printf("step3: audio pipeline done\r\n");

    printf("\r\n=======================================================\r\n");
    printf("   Parking Detect & Audio Pass System Online\r\n");
    printf("   - Total Slots: %d\r\n", CDS_CHANNEL_COUNT);
    printf("   - Per-Slot Adaptive Thresholds Applied\r\n");
    printf("=======================================================\r\n\r\n");
}

/* B-01 슬롯 전용 주차 감지 디버그 모니터 */
static void printB01Monitor(void) {
    bool is_occupied = adcIsOccupied(SLOT_B01);
    uint16_t raw_adc = adcGetCdsRaw(SLOT_B01);
    float volt = adcGetCdsVolt(SLOT_B01);

    printf("[B-01 MONITOR] Status: %-8s [%s] | Raw ADC: %4u (%.2fV) | [Thresh: EMP <= 2600 < ... < 3000 <= OCC]\r\n",
           is_occupied ? "OCCUPIED" : "EMPTY",
           is_occupied ? "#" : " ",
           raw_adc,
           volt);
}

void appMain(void) {
    static uint16_t prev_slot_mask = 0xFFFF;
    static bool prev_b01_status = false;
    static uint32_t prev_dbg_tick = 0;
    static uint32_t prev_plot_tick = 0;
    uint32_t now = HAL_GetTick();

    /* 1. ADC DMA 주차 센서 업데이트 */
    adcUpdate();

    /* 1.5 UART RX DMA 처리 (지속적으로 호출) */
    commUartProcessRx();

    /* 2. Teleplot 파형 모니터링 (100ms 주기 - 전체 슬롯 ADC 그래프용) */
    if (now - prev_plot_tick >= 100) {
        prev_plot_tick = now;
        // 모든 센서의 저항값(ADC Raw)을 Teleplot에 출력하여 튜닝할 수 있게 함
        for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
            ParkingSlotID_t slot = (ParkingSlotID_t)i;
            const char* name = parkingGetSlotName(slot);
            // "A-01"을 "A1_ADC" 형식으로 변환하여 출력 (이름 특수문자 방지)
            printf(">%c%c_ADC:%u\n", name[0], name[3], adcGetCdsRaw(slot));
        }
    }

    /* 3. 10개 슬롯 상태 취합 */
    uint16_t current_slot_mask = 0;
    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        if (adcIsOccupied((ParkingSlotID_t)i)) {
            current_slot_mask |= (uint16_t)(1U << i);
        }
    }

    /* 4. B-01 상태 변경 감지 및 이벤트 로그 출력 */
    bool current_b01_status = adcIsOccupied(SLOT_B01);
    bool state_changed = (current_slot_mask != prev_slot_mask);

    if (state_changed) {
        if (prev_slot_mask != 0xFFFF && current_b01_status != prev_b01_status) {
            printf("\r\n=======================================================\r\n");
            printf(">>> [B-01 EVENT] %s -> %s (ADC Raw: %u)\r\n",
                   prev_b01_status ? "OCCUPIED" : "EMPTY",
                   current_b01_status ? "OCCUPIED (차량 입차)" : "EMPTY (차량 출차)",
                   adcGetCdsRaw(SLOT_B01));
            printf(">>> Sent $PARK Packet (Mask: 0x%04X)\r\n", current_slot_mask);
            printf("=======================================================\r\n\r\n");
        }
        prev_slot_mask = current_slot_mask;
        prev_b01_status = current_b01_status;
        commUartSendParkingStatus();
    }

    /* 4.5 1초 주기 동기화 통신 (InfoMark 초기 부팅 대응) */
    static uint32_t prev_sync_tick = 0;
    if (now - prev_sync_tick >= 1000) {
        prev_sync_tick = now;
        if (!state_changed) {
            commUartSendParkingStatus();
        }
    }

    /* 5. 500ms 주기 B-01 집중 모니터링 콘솔 출력 */
    if (now - prev_dbg_tick >= 500) {
        prev_dbg_tick = now;
        printB01Monitor();
    }
}