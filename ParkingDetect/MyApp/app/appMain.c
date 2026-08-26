#include "appMain.h"
#include "sensorADC.h"
#include "usart.h"
#include <stdio.h>

#ifdef __GNUC__
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
#endif

void appInit(void) {
    adcInit();
    printf("\r\n========================================\r\n");
    printf("  6CH CDS Parking Detection System\r\n");
    printf("========================================\r\n");
}

void appMain(void) {
    static uint32_t prev_tick = 0;
    uint32_t current_tick = HAL_GetTick();

    /* 백그라운드 DMA 완료 및 상태 업데이트 */
    adcUpdate();

    /* 1초(1000ms) 주기로 6개 슬롯의 Raw 값, 전압, 주차 상태 출력 */
    if (current_tick - prev_tick >= 1000) {
        prev_tick = current_tick;

        const char *pin_names[CDS_CHANNEL_COUNT] = {"PA0", "PA1", "PA4", "PB0", "PC0", "PC1"};

        printf("---------------------------------------------------\r\n");
        for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
            CdsChannel_t ch = (CdsChannel_t)i;
            bool occupied = adcIsOccupied(ch);

            printf("[%s] Raw: %4u | Volt: %.2fV | Status: %s\r\n",
                   pin_names[i],
                   adcGetCdsRaw(ch),
                   adcGetCdsVolt(ch),
                   occupied ? "[ OCCUPIED ]" : "[  EMPTY   ]");
        }
    }
}