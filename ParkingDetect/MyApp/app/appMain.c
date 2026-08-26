#include "appMain.h"
#include "sensorADC.h"
#include "UARTTX.h"
#include <stdio.h>

void appInit(void) {
    adcInit();
    commUartInit();
    printf("=== 6-Slot Parking Management System ===\r\n");
}

void appMain(void) {
    static uint32_t prev_tx_tick = 0;
    static uint32_t prev_dbg_tick = 0;
    uint32_t now = HAL_GetTick();

    /* ADC DMA 완료 체크 및 주차 판별 FSM 갱신 */
    adcUpdate();

    /* 100ms 주기로 상대 보드에 주차 상태 전송 */
    if (now - prev_tx_tick >= 100) {
        prev_tx_tick = now;
        commUartSendParkingStatus();
    }

    /* 1초 주기로 PC 디버그 터미널 출력 */
    if (now - prev_dbg_tick >= 1000) {
        prev_dbg_tick = now;

        printf("---------------------------------------------------\r\n");
        for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
            ParkingSlotID_t slot = (ParkingSlotID_t)i;
            printf("Slot [%s] | Raw: %4u | Status: %s\r\n",
                   parkingGetSlotName(slot),
                   adcGetCdsRaw(slot),
                   adcIsOccupied(slot) ? "[ OCCUPIED ]" : "[  EMPTY   ]");
        }
    }
}