#include "UARTTX.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* CubeMX에서 생성된 UART 핸들러 연결 */
#define COMM_UART   huart1  /* 상대방 보드 연결 UART (필요시 변경) */
#define DEBUG_UART  huart2  /* ST-Link VCP PC 터미널 디버그 UART (필요시 변경) */

extern UART_HandleTypeDef COMM_UART;
extern UART_HandleTypeDef DEBUG_UART;

/* printf를 DEBUG_UART로 리다이렉션 */
#ifdef __GNUC__
int __io_putchar(int ch) {
    HAL_UART_Transmit(&DEBUG_UART, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
#endif

void commUartInit(void) {
    printf("[UART] Comm Module Initialized\r\n");
}

void commUartSendParkingStatus(void) {
    char tx_buf[128];
    int offset = snprintf(tx_buf, sizeof(tx_buf), "$PARK");

    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        ParkingSlotID_t slot = (ParkingSlotID_t)i;
        offset += snprintf(tx_buf + offset, sizeof(tx_buf) - offset, ",%s:%d",
                           parkingGetSlotName(slot),
                           adcIsOccupied(slot) ? 1 : 0);
    }
    snprintf(tx_buf + offset, sizeof(tx_buf) - offset, "\n");

    HAL_UART_Transmit(&COMM_UART, (uint8_t *)tx_buf, (uint16_t)strlen(tx_buf), 50);
}

void commUartSendSlotEvent(ParkingSlotID_t slot, ParkingStatus_t status) {
    char tx_buf[64];
    snprintf(tx_buf, sizeof(tx_buf), "$EVENT,%s:%s\n",
             parkingGetSlotName(slot),
             (status == PARKING_OCCUPIED) ? "OCCUPIED" : "EMPTY");

    HAL_UART_Transmit(&COMM_UART, (uint8_t *)tx_buf, (uint16_t)strlen(tx_buf), 50);
}