#ifndef _UI_H_
#define _UI_H_

#include <stdint.h>

// Logical landscape coordinates: 296 wide x 128 high.
// Weekday uses 0=Sun, 1=Mon, ... 6=Sat.
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  weekday;
    uint8_t  hour;
    uint8_t  minute;
} UI_DateTime;

typedef struct {
    char     novel[256];
    uint16_t chapter;
    uint16_t chapter_count;
    uint16_t page;
} UI_LastRead;

typedef enum {
    UI_BUTTON_NONE = 0,
    UI_BUTTON_SHORT,   // single click  → confirm / select
    UI_BUTTON_DOUBLE,  // double click  → next item
    UI_BUTTON_LONG     // long press    → back to main
} UI_ButtonEvent;

void UI_Init(const UI_DateTime *now, const UI_LastRead *last);
void UI_UpdateClock(const UI_DateTime *now);
void UI_GetCurrentTime(UI_DateTime *out_time);
void UI_SetLastRead(const UI_LastRead *last);
void UI_HandleButton(UI_ButtonEvent event);
void UI_SetSdStatus(int mounted, float capacity_gb);

#endif
