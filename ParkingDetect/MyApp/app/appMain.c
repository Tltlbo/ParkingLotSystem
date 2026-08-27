#include "appMain.h"
#include "audioPass.h"
#include "sensorADC.h"
#include "UARTTX.h"
#include <stdio.h>

extern int32_t mic_dma_buf[AUDIO_BUF_SIZE * 2];

void appInit(void) {
    adcInit();
    commUartInit();
    audioInit();
    printf("=== Parking Detect & Audio Pass System Online ===\r\n");
}

void appMain(void) {
    static uint8_t prev_slot_mask = 0xFF;
    static uint32_t prev_dbg_tick = 0;
    static uint32_t prev_plot_tick = 0;
    uint32_t now = HAL_GetTick();

    /* 1. ADC DMA 주차 센서 업데이트 */
    adcUpdate();

    /* 2. Teleplot 파형 모니터링 (50ms 주기) */
    if (now - prev_plot_tick >= 50) {
        prev_plot_tick = now;
        printf(">mic_peak:%ld\n>mic_sample:%d\n", audioGetPeakAmplitude(), audioGetLastSample());
    }

    /* 3. 주차 슬롯 상태 취합 */
    uint8_t current_slot_mask = 0;
    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        if (adcIsOccupied((ParkingSlotID_t)i)) {
            current_slot_mask |= (1 << i);
        }
    }

    /* 4. 상태 변경 시에만 패킷 송신 */
    if (current_slot_mask != prev_slot_mask) {
        prev_slot_mask = current_slot_mask;
        commUartSendParkingStatus();
        printf("[EVENT] State Changed! Mask: 0x%02X\r\n", current_slot_mask);
    }

    /* 5. 500ms 주기 콘솔 디버그 출력 */
    if (now - prev_dbg_tick >= 500) {
        prev_dbg_tick = now;
        printf("[MIC] Raw: 0x%08lX | Sample: %6d | Peak: %5ld | [SLOT] 0x%02X\r\n",
               (uint32_t)mic_dma_buf[0],
               audioGetLastSample(),
               audioGetPeakAmplitude(),
               current_slot_mask);
    }
}