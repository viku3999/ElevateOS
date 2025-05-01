#include "oled.h"
#include "i2c.h"
#include <string.h>
#include <unistd.h>

// SSD1106 data buffer
static uint8_t SSD1106_Buffer[SSD1106_WIDTH * SSD1106_HEIGHT / 8];

// Private SSD1106 structure
typedef struct {
    uint16_t CurrentX;
    uint16_t CurrentY;
    uint8_t Initialized;
} SSD1106_t;

// Private variable
static SSD1106_t SSD1106;

#define SSD1106_DEACTIVATE_SCROLL 0x2E // Stop scroll

// Write command helper macro
#define SSD1106_WRITECOMMAND(fd, command) i2c_write_byte(fd, SSD1106_I2C_ADDR, 0x00, (command))

uint8_t SSD1106_init(int fd) {
    if (fd < 0) return 0;

    // Init sequence
    SSD1106_WRITECOMMAND(fd, 0xAE); // Display off
    SSD1106_WRITECOMMAND(fd, 0x20); // Set Memory Addressing Mode
    SSD1106_WRITECOMMAND(fd, 0x10); // Page Addressing Mode
    SSD1106_WRITECOMMAND(fd, 0xB0); // Set Page Start Address
    SSD1106_WRITECOMMAND(fd, 0xC8); // Set COM Output Scan Direction
    SSD1106_WRITECOMMAND(fd, 0x00); // Set low column address
    SSD1106_WRITECOMMAND(fd, 0x10); // Set high column address
    SSD1106_WRITECOMMAND(fd, 0x40); // Set start line address
    SSD1106_WRITECOMMAND(fd, 0x81); // Set contrast control
    SSD1106_WRITECOMMAND(fd, 0xFF); // Max contrast
    SSD1106_WRITECOMMAND(fd, 0xA1); // Segment remap
    SSD1106_WRITECOMMAND(fd, 0xA6); // Normal display
    SSD1106_WRITECOMMAND(fd, 0xA8); // Set multiplex ratio
    SSD1106_WRITECOMMAND(fd, 0x3F); // 1/64 duty
    SSD1106_WRITECOMMAND(fd, 0xA4); // Output follows RAM
    SSD1106_WRITECOMMAND(fd, 0xD3); // Set display offset
    SSD1106_WRITECOMMAND(fd, 0x00); // No offset
    SSD1106_WRITECOMMAND(fd, 0xD5); // Set display clock
    SSD1106_WRITECOMMAND(fd, 0xF0); // Divide ratio
    SSD1106_WRITECOMMAND(fd, 0xD9); // Set pre-charge period
    SSD1106_WRITECOMMAND(fd, 0x22); 
    SSD1106_WRITECOMMAND(fd, 0xDA); // Set COM pins
    SSD1106_WRITECOMMAND(fd, 0x12);
    SSD1106_WRITECOMMAND(fd, 0xDB); // Set VCOMH
    SSD1106_WRITECOMMAND(fd, 0x20); // 0.77xVcc
    SSD1106_WRITECOMMAND(fd, 0x8D); // Charge pump
    SSD1106_WRITECOMMAND(fd, 0x14); // Enable charge pump
    SSD1106_WRITECOMMAND(fd, 0xAF); // Display on
    SSD1106_WRITECOMMAND(fd, SSD1106_DEACTIVATE_SCROLL);

    SSD1106_fill(SSD1106_COLOR_BLACK);
    SSD1106_update_screen(fd);

    SSD1106.CurrentX = 0;
    SSD1106.CurrentY = 0;
    SSD1106.Initialized = 1;

    return SSD1106.Initialized;
}

void SSD1106_update_screen(int fd) {
    uint8_t m;
    for (m = 0; m < 8; m++) {
        SSD1106_WRITECOMMAND(fd, 0xB0 + m);
        SSD1106_WRITECOMMAND(fd, 0x00);
        SSD1106_WRITECOMMAND(fd, 0x10);
        i2c_write_multi(fd, SSD1106_I2C_ADDR, 0x40, 
                       &SSD1106_Buffer[SSD1106_WIDTH * m], 
                       SSD1106_WIDTH);
    }
}

void SSD1106_fill(SSD1106_COLOR_t color) {
    memset(SSD1106_Buffer, (color == SSD1106_COLOR_BLACK) ? 0x00 : 0xFF, sizeof(SSD1106_Buffer));
}

void SSD1106_draw_pixel(uint16_t x, uint16_t y, SSD1106_COLOR_t color) {
    if (x >= SSD1106_WIDTH || y >= SSD1106_HEIGHT) return;
    
    if (color == SSD1106_COLOR_WHITE) {
        SSD1106_Buffer[x + (y / 8) * SSD1106_WIDTH] |= 1 << (y % 8);
    } else {
        SSD1106_Buffer[x + (y / 8) * SSD1106_WIDTH] &= ~(1 << (y % 8));
    }
}

void SSD1106_gotoXY(uint16_t x, uint16_t y) {
    SSD1106.CurrentX = x;
    SSD1106.CurrentY = y;
}

char SSD1106_putc(int fd, char ch, FontDef *Font, SSD1106_COLOR_t color) {
    uint32_t i, b, j;

    if (SSD1106_WIDTH <= (SSD1106.CurrentX + Font->FontWidth) ||
        SSD1106_HEIGHT <= (SSD1106.CurrentY + Font->FontHeight)) {
        return 0;
    }

    for (i = 0; i < Font->FontHeight; i++) {
        b = Font->data[(ch - 32) * Font->FontHeight + i];
        for (j = 0; j < Font->FontWidth; j++) {
            if ((b << j) & 0x8000) {
                SSD1106_draw_pixel(SSD1106.CurrentX + j, (SSD1106.CurrentY + i), color);
            } else {
                SSD1106_draw_pixel(SSD1106.CurrentX + j, (SSD1106.CurrentY + i), (SSD1106_COLOR_t)!color);
            }
        }
    }

    SSD1106.CurrentX += Font->FontWidth;
    return ch;
}

char SSD1106_puts(int fd, char *str, FontDef *Font, SSD1106_COLOR_t color) {
    while (*str) {
        if (SSD1106_putc(fd, *str, Font, color) != *str) {
            return *str;
        }
        str++;
    }
    return *str;
}

void SSD1106_printDigit(int fd, uint8_t digit, FontDef *Font, SSD1106_COLOR_t color) {
    // Ensure digit is between 0-4
    if (digit > 4) {
        digit = 0;
    }
    
    // Simple lookup table for single-digit characters
    const char digits[] = {'0','1','2','3','4'};
    
    // Create a null-terminated string with the digit
    char str[2] = {digits[digit], '\0'};
    
    // Display the digit
    SSD1106_puts(fd, str, Font, color);
}

void SSD1106_clear_screen(int fd) {
    SSD1106_fill(SSD1106_COLOR_BLACK);
    SSD1106_update_screen(fd);
}

void SSD1106_clear_line(int fd) {
    char empty_line[21] = "                    "; // 20 spaces
    uint16_t current_y = SSD1106.CurrentY;
    SSD1106_gotoXY(0, current_y);
    SSD1106_puts(fd, empty_line, &Font_7x10, SSD1106_COLOR_WHITE);
    SSD1106_update_screen(fd);
    SSD1106_gotoXY(0, current_y);
}