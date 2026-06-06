#ifndef __FONT_GB2312_H
#define __FONT_GB2312_H

#include <stdint.h>

typedef struct {
    uint16_t unicode;
    uint8_t bitmap[32];
} Glyph16x16;

extern const Glyph16x16 FONT_GB2312[];
extern const int FONT_GB2312_SIZE;

const uint8_t* font_get_glyph(uint16_t unicode);

#endif
