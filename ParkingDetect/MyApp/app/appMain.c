#include "appMain.h"
#include "sensorADC.h"
#include "UARTTX.h"
#include <stdio.h>

void appInit(void) {
    adcInit();
    commUartInit();
    printf("=== 6-Slot Parking Management System (Event-Driven) ===\r\n");
}

void appMain(void) {
    /* 이전 비트마스크 상태 (초기값을 0xFF로 두어 부팅 직후 첫 상태는 무조건 1회 전송) */
    static uint8_t prev_slot_mask = 0xFF;
    static uint32_t prev_dbg_tick = 0;
    uint32_t now = HAL_GetTick();

    /* ADC DMA 완료 체크 및 주차 판별 FSM 갱신 */
    adcUpdate();

    /* 1. 현재 6개 슬롯 점유 상태를 비트마스크로 취합 (Bit 0~5) */
    uint8_t current_slot_mask = 0;
    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        if (adcIsOccupied((ParkingSlotID_t)i)) {
            current_slot_mask |= (1 << i);
        }
    }

    /* 2. 주차 상태가 변경된 경우에만 즉시 UART 전송 */
    if (current_slot_mask != prev_slot_mask) {
        prev_slot_mask = current_slot_mask;

        /* 상대방 보드로 $PARK 패킷 송신 */
        commUartSendParkingStatus();

        /* PC 디버그 콘솔 알림 */
        printf("[EVENT] State Changed! Mask: 0x%02X\r\n", current_slot_mask);
    }

    /* 3. 1초 주기 PC 디버그 터미널 상태 출력 (선택 사항) */
    // if (now - prev_dbg_tick >= 1000) {
    //     prev_dbg_tick = now;

    //     printf("---------------------------------------------------\r\n");
    //     for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
    //         ParkingSlotID_t slot = (ParkingSlotID_t)i;
    //         printf("Slot [%s] | Raw: %4u | Status: %s\r\n",
    //                parkingGetSlotName(slot),
    //                adcGetCdsRaw(slot),
    //                adcIsOccupied(slot) ? "[ OCCUPIED ]" : "[  EMPTY   ]");
    //     }
    // }
}