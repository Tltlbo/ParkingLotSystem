#include "ssd1306.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

// Screen object
static SSD1306_t SSD1306;

static I2C_HandleTypeDef *s_hi2c;
static uint8_t s_ssd1306_i2c_addr;

// Send a byte to the command register
static void ssd1306_WriteCommand(uint8_t command) {
    HAL_I2C_Mem_Write(s_hi2c, s_ssd1306_i2c_addr, 0x00, 1, &command, 1, 10);
}

// Initialize the oled screen
void ssd1306_Init(I2C_HandleTypeDef *hi2c, uint8_t i2c_addr) {
    s_hi2c = hi2c;
    s_ssd1306_i2c_addr = i2c_addr;
    
    // Wait for screen power-on
    HAL_Delay(100);
    
    // 디바이스 응답 확인
    if (HAL_I2C_IsDeviceReady(s_hi2c, s_ssd1306_i2c_addr, 2, 20) != HAL_OK) {
        printf("[OLED] Device NOT responding at 0x%02X (7-bit: 0x%02X)!\r\n", s_ssd1306_i2c_addr, s_ssd1306_i2c_addr >> 1);
        return;
    }
    printf("[OLED] Device READY at 0x%02X (7-bit: 0x%02X)!\r\n", s_ssd1306_i2c_addr, s_ssd1306_i2c_addr >> 1);
    
    // Init LCD sequence
    ssd1306_WriteCommand(0xAE); // Display off
    ssd1306_WriteCommand(0xD5); // Set Display Clock Divide Ratio / Oscillator Frequency
    ssd1306_WriteCommand(0x80); // Default clock
    ssd1306_WriteCommand(0xA8); // Set Multiplex Ratio
    ssd1306_WriteCommand(0x3F); // 64MUX
    ssd1306_WriteCommand(0xD3); // Set Display Offset
    ssd1306_WriteCommand(0x00); // 0 offset
    ssd1306_WriteCommand(0x40); // Set Display Start Line (0x40)
    ssd1306_WriteCommand(0x8D); // Charge Pump Setting
    ssd1306_WriteCommand(0x14); // Enable Charge Pump (0x14) - 필수!
    ssd1306_WriteCommand(0x20); // Memory Addressing Mode
    ssd1306_WriteCommand(0x02); // 0x02 = Page Addressing Mode (SSD1306 / SH1106 표준 호환)
    ssd1306_WriteCommand(0xA1); // Set Segment Re-map
    ssd1306_WriteCommand(0xC8); // Set COM Output Scan Direction
    ssd1306_WriteCommand(0xDA); // Set COM Pins Hardware Configuration
    ssd1306_WriteCommand(0x12); // 0x12 for 128x64
    ssd1306_WriteCommand(0x81); // Set Contrast Control
    ssd1306_WriteCommand(0xCF); // High contrast
    ssd1306_WriteCommand(0xD9); // Set Pre-charge Period
    ssd1306_WriteCommand(0xF1);
    ssd1306_WriteCommand(0xDB); // Set VCOMH Deselect Level
    ssd1306_WriteCommand(0x40);
    ssd1306_WriteCommand(0xA4); // Resume to RAM content display
    ssd1306_WriteCommand(0xA6); // Normal Display
    ssd1306_WriteCommand(0xAF); // Display ON
    
    SSD1306.Initialized = 1;
    
    // Clear screen
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
    
    // Set default values for screen object
    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;
}

// Fill the whole screen with the given color
void ssd1306_Fill(SSD1306_COLOR color) {
    uint32_t i;
    for(i = 0; i < sizeof(SSD1306_Buffer); i++) {
        SSD1306_Buffer[i] = (color == Black) ? 0x00 : 0xFF;
    }
}

// Write the screenbuffer with changed to the screen
void ssd1306_UpdateScreen(void) {
    if (!SSD1306.Initialized && s_hi2c == NULL) return;

    for (uint8_t i = 0; i < 8; i++) {
        ssd1306_WriteCommand(0xB0 + i); // Set Page Start Address
        ssd1306_WriteCommand(0x00);      // Set Lower Column Start Address
        ssd1306_WriteCommand(0x10);      // Set Higher Column Start Address
        HAL_I2C_Mem_Write(s_hi2c, s_ssd1306_i2c_addr, 0x40, 1, &SSD1306_Buffer[SSD1306_WIDTH * i], SSD1306_WIDTH, 100);
    }
}

// Draw one pixel in the screenbuffer
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color) {
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return; // Don't write outside the buffer
    }
    
    if (color == White) {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    } else {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}

char ssd1306_WriteChar(char ch, FontDef Font, SSD1306_COLOR color) {
    uint32_t i, b, j;
    
    if (SSD1306_WIDTH <= (SSD1306.CurrentX + Font.FontWidth) ||
        SSD1306_HEIGHT <= (SSD1306.CurrentY + Font.FontHeight)) {
        return 0; // Error write outside the buffer
    }
    
    // Use the font to write
    for (i = 0; i < Font.FontHeight; i++) {
        b = Font.data[(ch - 32) * Font.FontHeight + i];
        
        // Font6x8 배열 데이터가 MSB가 아닌 LSB에 정렬되어 있는 버그 보정
        if (Font.FontWidth == 6) {
            b <<= 8;
        }

        for (j = 0; j < Font.FontWidth; j++) {
            if ((b << j) & 0x8000) {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR) color);
            } else {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR)!color);
            }
        }
    }
    SSD1306.CurrentX += Font.FontWidth;
    return ch;
}

char ssd1306_WriteString(char* str, FontDef Font, SSD1306_COLOR color) {
    while (*str) {
        if (ssd1306_WriteChar(*str, Font, color) != *str) {
            return *str; // Error
        }
        str++;
    }
    return *str;
}

void ssd1306_SetCursor(uint8_t x, uint8_t y) {
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}

// Draw line
void ssd1306_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        ssd1306_DrawPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// Draw rectangle
void ssd1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, SSD1306_COLOR color) {
    ssd1306_DrawLine(x, y, x + w - 1, y, color);
    ssd1306_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    ssd1306_DrawLine(x, y, x, y + h - 1, color);
    ssd1306_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
}

// Fill rectangle
void ssd1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, SSD1306_COLOR color) {
    for (uint8_t i = x; i < x + w; i++) {
        for (uint8_t j = y; j < y + h; j++) {
            ssd1306_DrawPixel(i, j, color);
        }
    }
}
