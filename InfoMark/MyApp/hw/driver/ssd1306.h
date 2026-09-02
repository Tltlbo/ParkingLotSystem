#ifndef __SSD1306_H__
#define __SSD1306_H__

#include "main.h"
#include "ssd1306_fonts.h"

// SSD1306 OLED height in pixels
#ifndef SSD1306_HEIGHT
#define SSD1306_HEIGHT          64
#endif

// SSD1306 width in pixels
#ifndef SSD1306_WIDTH
#define SSD1306_WIDTH           128
#endif

// Some OLEDs don't display anything in first two columns.
#ifndef SSD1306_BUFFER_SIZE
#define SSD1306_BUFFER_SIZE   SSD1306_WIDTH * SSD1306_HEIGHT / 8
#endif

// Enumeration for screen colors
typedef enum {
    Black = 0x00, /*!< Black color, no pixel */
    White = 0x01  /*!< Pixel is set. Color depends on OLED */
} SSD1306_COLOR;

// Struct to store transformations
typedef struct {
    uint16_t CurrentX;
    uint16_t CurrentY;
    uint8_t Inverted;
    uint8_t Initialized;
} SSD1306_t;

// Procedure definitions
void ssd1306_Init(I2C_HandleTypeDef *hi2c, uint8_t i2c_addr);
void ssd1306_UpdateScreen(void);
void ssd1306_Fill(SSD1306_COLOR color);
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color);
char ssd1306_WriteChar(char ch, FontDef Font, SSD1306_COLOR color);
char ssd1306_WriteString(char* str, FontDef Font, SSD1306_COLOR color);
void ssd1306_SetCursor(uint8_t x, uint8_t y);
void ssd1306_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color);
void ssd1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, SSD1306_COLOR color);
void ssd1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, SSD1306_COLOR color);

#endif // __SSD1306_H__
