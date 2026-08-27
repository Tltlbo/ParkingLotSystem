#ifndef MYLCD_H_
#define MYLCD_H_

#include "main.h"
#include <stdint.h>

#define I2C_LCD_ADDR (0x27 << 1)

// uint8_t addr 매개변수 추가
void lcdInit(I2C_HandleTypeDef *hi2c, uint8_t addr);
void lcdSendCmd(char cmd);
void lcdSendData(char data);
void lcdSendString(char *str);
void lcdSetCursor(uint8_t row, uint8_t col);
void lcdClear(void);

#endif /* MYLCD_H_ */