#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define AUDIO_BUF_SIZE      256   /* Ping-Pong 버퍼 하프 사이즈 */
#define PWM_ARR_MAX         839   /* TIM1 Period (ARR) */
#define AUDIO_FRAME_SAMPLES 128

void audioInit(void);
void audioProcessHalf(void);
void audioProcessFull(void);
void audioPushToRingBuffer(const int16_t *samples, uint16_t count);

int32_t audioGetPeakAmplitude(void);
int16_t audioGetLastSample(void);