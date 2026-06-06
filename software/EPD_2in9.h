#ifndef __EPD_2IN9_H_
#define __EPD_2IN9_H_

#include "DEV_Config.h"

#define EPD_2IN9_FULL 0
#define EPD_2IN9_PART 1

uint8_t EPD_2IN9_Init(uint8_t Mode);
uint8_t EPD_2IN9_Clear(void);
void    EPD_2IN9_Display(const char *text);
void    EPD_2IN9_DisplayLines(const char *line1, const char *line2, const char *line3);
void    EPD_2IN9_DisplayWithBG(const uint8_t *bg_bitmap, const char *line1, const char *line2, const char *line3);
void    EPD_2IN9_DisplayWindow(const uint8_t *full_frame, int x, int y, int w, int h);
void    EPD_2IN9_Sleep(void);

// Drawing primitives (draw into framebuffer, call Flush after)
void    EPD_DrawPixel(int x, int y, int black);
void    EPD_DrawHLine(int x, int y, int w, int black);
void    EPD_DrawVLine(int x, int y, int h, int black);
void    EPD_DrawBox(int x, int y, int w, int h, int black);
void    EPD_DrawText(int x, int y, const char *s);
void    EPD_FbClear(void);
void    EPD_Flush(void);

#endif
