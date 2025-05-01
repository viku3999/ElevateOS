#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include "fonts.h"

#define SSD1106_WIDTH 128
#define SSD1106_HEIGHT 64
#define SSD1106_I2C_ADDR 0x3C

typedef enum {
    SSD1106_COLOR_BLACK = 0x00,
    SSD1106_COLOR_WHITE = 0x01
} SSD1106_COLOR_t;

uint8_t SSD1106_init(int fd);
void SSD1106_update_screen(int fd);
void SSD1106_fill(SSD1106_COLOR_t color);
void SSD1106_draw_pixel(uint16_t x, uint16_t y, SSD1106_COLOR_t color);
void SSD1106_gotoXY(uint16_t x, uint16_t y);
char SSD1106_putc(int fd, const char ch, FontDef *Font, SSD1106_COLOR_t color);
char SSD1106_puts(int fd, const char *str, FontDef *Font, SSD1106_COLOR_t color);
void SSD1106_printDigit(int fd, uint8_t digit, FontDef *Font, SSD1106_COLOR_t color);
void SSD1106_clear_screen(int fd);
void SSD1106_clear_line(int fd);

#endif