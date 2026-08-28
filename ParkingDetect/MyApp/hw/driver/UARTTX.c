#include "UARTTX.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define COMM_UART   huart1
#define DEBUG_UART  huart2

extern UART_HandleTypeDef COMM_UART;
extern UART_HandleTypeDef DEBUG_UART;

/* 오디오 프레임 송신용 전용 버퍼 (Ping-Pong 2개로 전송 덮어쓰기 방지) */
static uint8_t audio_frame_buf[2][AUDIO_PACKET_TOTAL];
static uint8_t frame_buf_idx = 0;

#ifdef __GNUC__
int __io_putchar(int ch) {
    HAL_UART_Transmit(&DEBUG_UART, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
#endif

void commUartInit(void) {
    printf("[UART] Comm Module Initialized (Multiplexing Mode)\r\n");
}

/* 바이너리 오디오 패킷 패킹 및 DMA 송신 */
void commUartSendAudioFrame(const int16_t *pcm_samples) {
    uint8_t *p_buf = audio_frame_buf[frame_buf_idx];
    frame_buf_idx = (frame_buf_idx + 1) % 2; // 다음 버퍼로 스위칭

    /* 1. 고유 프레임 헤더 (0xAA, 0x55) */
    p_buf[0] = 0xAA;
    p_buf[1] = 0x55;

    /* 2. Payload 길이 (256 바이트 -> 0x0100) */
    p_buf[2] = (uint8_t)(AUDIO_PAYLOAD_SIZE & 0xFF);
    p_buf[3] = (uint8_t)((AUDIO_PAYLOAD_SIZE >> 8) & 0xFF);

    /* 3. PCM 데이터 복사 */
    memcpy(&p_buf[4], (const uint8_t *)pcm_samples, AUDIO_PAYLOAD_SIZE);

    /* 4. 단순 체크섬(XOR) */
    uint8_t checksum = 0;
    for (int i = 0; i < 4 + AUDIO_PAYLOAD_SIZE; i++) {
        checksum ^= p_buf[i];
    }
    p_buf[4 + AUDIO_PAYLOAD_SIZE] = checksum;

    /* 5. Non-Blocking DMA 송신 (이전 송신 완료 상태일 때만) */
    if (COMM_UART.gState == HAL_UART_STATE_READY) {
        HAL_UART_Transmit_DMA(&COMM_UART, p_buf, AUDIO_PACKET_TOTAL);
    }
}

/* 주차 패킷 전송: 오디오 스트림 깨짐을 최소화하기 위해 논블로킹 체크 */
void commUartSendParkingStatus(void) {
    static char tx_buf[256];
    int offset = snprintf(tx_buf, sizeof(tx_buf), "$PARK");

    for (int i = 0; i < CDS_CHANNEL_COUNT; i++) {
        ParkingSlotID_t slot = (ParkingSlotID_t)i;
        offset += snprintf(tx_buf + offset, sizeof(tx_buf) - offset, ",%s:%d",
                           parkingGetSlotName(slot),
                           adcIsOccupied(slot) ? 1 : 0);
    }
    snprintf(tx_buf + offset, sizeof(tx_buf) - offset, "\n");

    /* 오디오 DMA 송신이 끝난 순간 빠르게 송신 (Timeout 5ms) */
    HAL_UART_Transmit(&COMM_UART, (uint8_t *)tx_buf, (uint16_t)strlen(tx_buf), 50);
}

void commUartSendSlotEvent(ParkingSlotID_t slot, ParkingStatus_t status) {
    char tx_buf[64];
    snprintf(tx_buf, sizeof(tx_buf), "$EVENT,%s:%s\n",
             parkingGetSlotName(slot),
             (status == PARKING_OCCUPIED) ? "OCCUPIED" : "EMPTY");

    HAL_UART_Transmit(&COMM_UART, (uint8_t *)tx_buf, (uint16_t)strlen(tx_buf), 5);
}