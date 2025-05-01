#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>

typedef struct {
    uint8_t FontWidth;
    uint8_t FontHeight;
    const uint16_t *data;
} FontDef;

extern FontDef Font_7x10;

#endif