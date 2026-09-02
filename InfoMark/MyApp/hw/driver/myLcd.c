#include "myLcd.h"
#include "i2c.h"

static I2C_HandleTypeDef *s_hi2c;
static uint8_t s_lcd_addr;

static void lcdWriteByte(char val, uint8_t rs) {
    uint8_t u = val & 0xF0;
    uint8_t l = (val << 4) & 0xF0;
    uint8_t data[4];
    
    // rs == 1 (Data: RS=1, BL=1 -> 0x0D / 0x09)
    // rs == 0 (Cmd:  RS=0, BL=1 -> 0x0C / 0x08)
    uint8_t en_high = rs ? 0x0D : 0x0C;
    uint8_t en_low  = rs ? 0x09 : 0x08;

    data[0] = u | en_high;
    data[1] = u | en_low;
    data[2] = l | en_high;
    data[3] = l | en_low;

    HAL_I2C_Master_Transmit(s_hi2c, s_lcd_addr, data, 4, 100);
}

void lcdSendCmd(char cmd)   { lcdWriteByte(cmd, 0); }
void lcdSendData(char data) { lcdWriteByte(data, 1); }

static void lcdSendNibble(uint8_t nibble) {
    uint8_t data[2] = { (nibble & 0xF0) | 0x0C, (nibble & 0xF0) | 0x08 };
    HAL_I2C_Master_Transmit(s_hi2c, s_lcd_addr, data, 2, 100);
}

void lcdInit(I2C_HandleTypeDef *hi2c, uint8_t addr) {
    s_hi2c = hi2c;
    s_lcd_addr = addr;

    HAL_Delay(50);
    lcdSendNibble(0x30); HAL_Delay(5);
    lcdSendNibble(0x30); HAL_Delay(1);
    lcdSendNibble(0x30); HAL_Delay(10);
    lcdSendNibble(0x20); HAL_Delay(10);

    lcdSendCmd(0x28); HAL_Delay(1);
    lcdSendCmd(0x08); HAL_Delay(1);
    lcdSendCmd(0x01); HAL_Delay(2);
    lcdSendCmd(0x06); HAL_Delay(1);
    lcdSendCmd(0x0C); HAL_Delay(1);
}

void lcdClear(void) {
    lcdSendCmd(0x01);
    HAL_Delay(5);
}

void lcdSetCursor(uint8_t row, uint8_t col) {
    uint8_t mask = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcdSendCmd(mask);
    HAL_Delay(1);
}

void lcdSendString(char *str) {
    while (*str) {
        lcdSendData(*str++);
    }
}