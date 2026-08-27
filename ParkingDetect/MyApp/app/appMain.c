#include "appMain.h"
#include "audioPass.h"
#include "sensorADC.h"
#include "UARTTX.h"
#include <stdio.h>

void appInit(void) {
    adcInit();
    commUartInit();
    audioInit();
    printf("=== 6-Slot Parking Management System (Event-Driven) ===\r\n");
}

void appMain(void) {
    /* 이전 비트마스크 상태 (부팅 직후 첫 상태는 무조건 1회 동기화) */
    static uint8_t prev_slot_mask = 0xFF;
    static uint32_t prev_dbg_tick = 0;
    static uint32_t prev_plot_tick = 0;
    uint32_t now = HAL_GetTick();

    /* 1. ADC DMA 완료 체크 및 주차 판별 FSM 갱신 */
    adcUpdate();

    /* 2. Teleplot 실시간 오디오 파형 출력 (30ms 주기) */
    if (now - prev_plot_tick >= 30) {
        prev_plot_tick = now;
        printf(">mic_peak:%ld\n", audioGetPeakAmplitude());
        printf(">mic_sample:%d\n", audioGetLastSample());
    }

    if (now - prev_plot_tick >= 50) {
        prev_plot_tick = now;
        printf(">mic_peak:%ld\n>mic_sample:%d\n", audioGetPeakAmplitude(), audioGetLastSample());
    }

    /* 3. 6개 슬롯 점유 상태 비트마스크 취합 (Bit 0~5) */
    uint8_t current_slot_mask = 0;
    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        if (adcIsOccupied((ParkingSlotID_t)i)) {
            current_slot_mask |= (1 << i);
        }
    }

    /* 4. 주차 상태 변경 감지 시 즉시 UART 전송 (이벤트 드리븐) */
    if (current_slot_mask != prev_slot_mask) {
        prev_slot_mask = current_slot_mask;

        /* 상대방 보드로 $PARK 패킷 송신 */
        commUartSendParkingStatus();

        /* PC 디버그 콘솔 알림 */
        printf("[EVENT] State Changed! Mask: 0x%02X\r\n", current_slot_mask);
    }

    /* 5. 500ms 주기 콘솔 디버그 출력 (주차 슬롯 및 마이크 피크 레벨) */
    if (now - prev_dbg_tick >= 500) {
        prev_dbg_tick = now;
        printf("[MIC LEVEL] Peak: %5ld | [SLOT MASK] 0x%02X\r\n", 
               audioGetPeakAmplitude(), current_slot_mask);
    }
}