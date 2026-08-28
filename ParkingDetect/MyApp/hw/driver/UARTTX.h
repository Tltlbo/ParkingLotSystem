#pragma once

#include "main.h"
#include "sensorADC.h"
#include <stdint.h>
#include <stdbool.h>

#define AUDIO_FRAME_SAMPLES   128  /* 1회 전송 모노 샘플 수 */
#define AUDIO_PAYLOAD_SIZE   (AUDIO_FRAME_SAMPLES * 2) /* 256 바이트 */
#define AUDIO_PACKET_TOTAL   (2 + 2 + AUDIO_PAYLOAD_SIZE + 1) /* 총 261 바이트 */

void commUartInit(void);
void commUartProcessRx(void);
void commUartSendParkingStatus(void);
void commUartSendSlotEvent(ParkingSlotID_t slot, ParkingStatus_t status);

/* 오디오 프레임 전송 함수 (헤더 패킹 + DMA 송신) */
void commUartSendAudioFrame(const int16_t *pcm_samples);