#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "DEV_Config.h"
#include "EPD_2in9.h"
#include "ui.h"
#include "ff.h"
#include <string.h>
#include <strings.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <bsp/board.h>
#include <tusb.h>

// ============================================================
// Button — GP0, active-low, internal pull-up.
//   Short  → confirm       Double → next        Long → back
// ============================================================
#define DEBOUNCE_MS  20
#define LONG_MS      800
#define DOUBLE_MS    400
#define TICK_MS      10

// ============================================================
// Simple click-counting detector (no complex state machine).
//   - Each debounced press increments click_count.
//   - Two presses within DOUBLE_MS → DOUBLE, reset count.
//   - One press + DOUBLE_MS silence after release → SHORT.
//   - Held >= LONG_MS → LONG, reset count.
// ============================================================
static uint8_t  db_stable     = 0;   // debounced button level
static uint8_t  db_last_raw   = 0;   // previous raw sample
static uint32_t db_changed_ms = 0;   // last raw-change timestamp
static uint32_t db_down_ms    = 0;   // last debounced-press time
static uint32_t db_up_ms      = 0;   // last debounced-release time
static uint8_t  click_cnt     = 0;   // clicks accumulated (0, 1, or 2)
static uint8_t  long_done     = 0;   // LONG already fired for this hold



// ============================================================
// Init
// ============================================================
static void btn_init(void)
{
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    uint8_t raw = (gpio_get(BUTTON_PIN) == 0) ? 1 : 0;
    db_last_raw   = raw;
    db_stable     = raw;
    db_changed_ms = 0;
    click_cnt     = 0;
    long_done     = 0;
}

// ============================================================
// Poll — returns one event per call (or NONE).
// ============================================================
static UI_ButtonEvent btn_poll(uint32_t now)
{
    uint8_t raw = (gpio_get(BUTTON_PIN) == 0) ? 1 : 0;

    // --- glitch filter ---
    if (raw != db_last_raw) {
        db_last_raw   = raw;
        db_changed_ms = now;
    }

    // --- debounced edge ---
    if (raw != db_stable &&
        (uint32_t)(now - db_changed_ms) >= DEBOUNCE_MS) {
        db_stable = raw;

        if (db_stable) {                     // PRESS
            db_down_ms = now;
            long_done  = 0;
        } else {                             // RELEASE
            db_up_ms = now;
            if (long_done) {
                click_cnt = 0;               // long already handled
                long_done = 0;
            } else {
                click_cnt++;
                if (click_cnt >= 2) {
                    click_cnt = 0;
                    return UI_BUTTON_DOUBLE;
                }
            }
        }
    }

    // --- LONG press while held ---
    if (db_stable && !long_done && click_cnt == 0 &&
        (uint32_t)(now - db_down_ms) >= LONG_MS) {
        long_done  = 1;
        click_cnt  = 0;
        return UI_BUTTON_LONG;
    }

    // --- SHORT timeout (no second click within DOUBLE_MS) ---
    if (click_cnt == 1 && !db_stable &&
        (uint32_t)(now - db_up_ms) >= DOUBLE_MS) {
        click_cnt = 0;
        return UI_BUTTON_SHORT;
    }

    return UI_BUTTON_NONE;
}

// ============================================================
// Event Queue for Timer ISR
// ============================================================
#define EVENT_QUEUE_SIZE 8
static volatile UI_ButtonEvent btn_queue[EVENT_QUEUE_SIZE];
static volatile uint8_t btn_q_head = 0;
static volatile uint8_t btn_q_tail = 0;

static void push_event(UI_ButtonEvent ev) {
    uint8_t next = (btn_q_head + 1) % EVENT_QUEUE_SIZE;
    if (next != btn_q_tail) {
        btn_queue[btn_q_head] = ev;
        btn_q_head = next;
    }
}

static UI_ButtonEvent pop_event(void) {
    if (btn_q_head == btn_q_tail) return UI_BUTTON_NONE;
    UI_ButtonEvent ev = btn_queue[btn_q_tail];
    btn_q_tail = (btn_q_tail + 1) % EVENT_QUEUE_SIZE;
    return ev;
}

static bool btn_timer_callback(struct repeating_timer *t) {
    static uint32_t isr_tick = 0;
    isr_tick += TICK_MS;
    UI_ButtonEvent ev = btn_poll(isr_tick);
    if (ev != UI_BUTTON_NONE) {
        push_event(ev);
    }
    return true;
}

// ============================================================
// Date / platform helpers
// ============================================================
static int is_leap(uint16_t y)
{ return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0); }

static uint8_t days_in_month(uint16_t y, uint8_t m)
{
    static const uint8_t d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && is_leap(y)) return 29;
    if (m >= 1 && m <= 12) return d[m - 1];
    return 30;
}

static void add_days(UI_DateTime *t, uint32_t days)
{
    while (days--) {
        t->day++;
        t->weekday = (uint8_t)((t->weekday + 1) % 7);
        if (t->day > days_in_month(t->year, t->month)) {
            t->day = 1; t->month++;
            if (t->month > 12) { t->month = 1; t->year++; }
        }
    }
}

static UI_DateTime g_base_time = {2026,6,5,5,12,0};
static uint8_t g_base_second = 0;
static uint32_t rtc_offset_ms = 0;

static void Platform_ReadRtc(UI_DateTime *o, uint32_t uptime_ms)
{
    uint32_t eff_uptime = (uptime_ms > rtc_offset_ms) ? (uptime_ms - rtc_offset_ms) : 0;
    uint32_t total_seconds = (eff_uptime / 1000UL) + g_base_second;
    uint32_t mins = total_seconds / 60UL;
    uint32_t tot  = (uint32_t)g_base_time.hour * 60 + g_base_time.minute + mins;
    uint32_t days = tot / 1440;
    tot %= 1440;
    *o = g_base_time;
    o->hour = (uint8_t)(tot / 60); o->minute = (uint8_t)(tot % 60);
    add_days(o, days);
}

static int get_json_int(const char *json, const char *key, int default_val) {
    char *ptr = (char*)strstr(json, key);
    if (ptr) {
        ptr = strchr(ptr, ':');
        if (ptr) return atoi(ptr + 1);
    }
    return default_val;
}

static void Platform_LoadLastRead(UI_LastRead *l)
{
    l->novel[0] = '\0';
    l->chapter = 1;
    l->chapter_count = 1;
    l->page = 1;

    FIL fp;
    if (f_open(&fp, "0:/settings.json", FA_READ) == FR_OK) {
        char buf[512];
        UINT br;
        f_read(&fp, buf, sizeof(buf) - 1, &br);
        buf[br] = '\0';
        f_close(&fp);

        char *ptr;
        if ((ptr = (char*)strstr(buf, "\"novel\""))) {
            ptr = strchr(ptr, ':');
            if (ptr && (ptr = strchr(ptr, '\"'))) {
                ptr++;
                char *end = strchr(ptr, '\"');
                if (end) {
                    int len = end - ptr;
                    if (len > 255) len = 255;
                    strncpy(l->novel, ptr, len);
                    l->novel[len] = '\0';
                }
            }
        }
        l->chapter = get_json_int(buf, "\"chapter\"", 1);
        l->chapter_count = get_json_int(buf, "\"chapter_count\"", 1);
        l->page = get_json_int(buf, "\"page\"", 1);

        g_base_time.year = get_json_int(buf, "\"year\"", 2026);
        g_base_time.month = get_json_int(buf, "\"month\"", 6);
        g_base_time.day = get_json_int(buf, "\"day\"", 5);
        g_base_time.weekday = get_json_int(buf, "\"weekday\"", 5);
        g_base_time.hour = get_json_int(buf, "\"hour\"", 12);
        g_base_time.minute = get_json_int(buf, "\"minute\"", 0);
    }
}

void UI_SaveSettingsToStorage(uint8_t idx)
{ printf("[BTN] save setting %u\r\n", idx); }

void UI_SaveLastReadToStorage(const UI_LastRead *l)
{
    UI_DateTime now;
    UI_GetCurrentTime(&now);
    
    FIL fp;
    if (f_open(&fp, "0:/settings.json", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\n"
            "  \"novel\": \"%s\",\n"
            "  \"chapter\": %d,\n"
            "  \"chapter_count\": %d,\n"
            "  \"page\": %d,\n"
            "  \"year\": %d,\n"
            "  \"month\": %d,\n"
            "  \"day\": %d,\n"
            "  \"weekday\": %d,\n"
            "  \"hour\": %d,\n"
            "  \"minute\": %d\n"
            "}\n",
            l->novel, l->chapter, l->chapter_count, l->page,
            now.year, now.month, now.day, now.weekday, now.hour, now.minute
        );
        UINT bw;
        f_write(&fp, buf, strlen(buf), &bw);
        f_close(&fp);
    }
}

// ============================================================
// Keyboard HID Task
// ============================================================
bool g_msc_enabled = false;

// TinyUSB HID callbacks
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

static char typing_buffer[512];
static int typing_index = 0;
static bool is_typing = false;
static bool is_typing_cmd = false;
static uint8_t cmd_state = 0;
static uint32_t typing_next_ms = 0;
static bool key_pressed = false;

void start_typing(const char *text) {
    strncpy(typing_buffer, text, 511);
    typing_buffer[511] = '\0';
    typing_index = 0;
    is_typing = true;
    is_typing_cmd = false;
    key_pressed = false;
    typing_next_ms = board_millis();
}

void start_typing_cmd(const char *cmd) {
    strncpy(typing_buffer, cmd, 511);
    typing_buffer[511] = '\0';
    typing_index = 0;
    is_typing = false;
    is_typing_cmd = true;
    cmd_state = 0;
    key_pressed = false;
    typing_next_ms = board_millis();
}

static uint8_t char_to_keycode(char c, uint8_t *modifier) {
    *modifier = 0;
    if (c >= 'a' && c <= 'z') return HID_KEY_A + (c - 'a');
    if (c >= 'A' && c <= 'Z') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_A + (c - 'A'); }
    if (c >= '1' && c <= '9') return HID_KEY_1 + (c - '1');
    if (c == '0') return HID_KEY_0;
    if (c == ' ') return HID_KEY_SPACE;
    if (c == '\n') return HID_KEY_ENTER;
    if (c == '\t') return HID_KEY_TAB;
    if (c == ';') return HID_KEY_SEMICOLON;
    if (c == ':') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_SEMICOLON; }
    if (c == '\'') return HID_KEY_APOSTROPHE;
    if (c == '"') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_APOSTROPHE; }
    if (c == '[') return HID_KEY_BRACKET_LEFT;
    if (c == '{') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_BRACKET_LEFT; }
    if (c == ']') return HID_KEY_BRACKET_RIGHT;
    if (c == '}') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_BRACKET_RIGHT; }
    if (c == '\\') return HID_KEY_BACKSLASH;
    if (c == '|') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_BACKSLASH; }
    if (c == ',') return HID_KEY_COMMA;
    if (c == '<') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_COMMA; }
    if (c == '.') return HID_KEY_PERIOD;
    if (c == '>') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_PERIOD; }
    if (c == '/') return HID_KEY_SLASH;
    if (c == '?') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_SLASH; }
    if (c == '-') return HID_KEY_MINUS;
    if (c == '_') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_MINUS; }
    if (c == '=') return HID_KEY_EQUAL;
    if (c == '+') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_EQUAL; }
    if (c == '`') return HID_KEY_GRAVE;
    if (c == '~') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_GRAVE; }
    
    // Fallback for some symbols
    if (c == '!') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_1; }
    if (c == '@') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_2; }
    if (c == '#') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_3; }
    if (c == '$') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_4; }
    if (c == '%') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_5; }
    if (c == '^') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_6; }
    if (c == '&') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_7; }
    if (c == '*') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_8; }
    if (c == '(') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_9; }
    if (c == ')') { *modifier = KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_0; }
    
    return 0; // Unknown
}

static void hid_task(void) {
    if (!is_typing && !is_typing_cmd) return;
    if (board_millis() < typing_next_ms) return;
    
    // Poll interval wait
    if (!tud_hid_ready()) return;

    if (is_typing_cmd) {
        if (cmd_state == 0) {
            if (key_pressed) {
                tud_hid_keyboard_report(1, 0, NULL);
                key_pressed = false;
                cmd_state = 1;
                typing_next_ms = board_millis() + 500; // wait 500ms for Run dialog
            } else {
                uint8_t keycode_arr[6] = {HID_KEY_R, 0, 0, 0, 0, 0};
                tud_hid_keyboard_report(1, KEYBOARD_MODIFIER_LEFTGUI, keycode_arr);
                key_pressed = true;
                typing_next_ms = board_millis() + 50;
            }
        } else if (cmd_state == 1) {
            // Typing characters
            if (key_pressed) {
                tud_hid_keyboard_report(1, 0, NULL);
                key_pressed = false;
                typing_next_ms = board_millis() + 2;
                typing_index++;
            } else {
                char c = typing_buffer[typing_index];
                if (c == '\0') {
                    cmd_state = 2; // move to ENTER state
                    typing_next_ms = board_millis() + 50;
                    return;
                }
                
                // Allow some standard characters missing from original
                if (c == '"') {
                    uint8_t keycode_arr[6] = {HID_KEY_APOSTROPHE, 0, 0, 0, 0, 0};
                    tud_hid_keyboard_report(1, KEYBOARD_MODIFIER_LEFTSHIFT, keycode_arr);
                    key_pressed = true;
                    typing_next_ms = board_millis() + 2;
                    return;
                }
                
                uint8_t modifier = 0;
                uint8_t keycode = char_to_keycode(c, &modifier);
                
                if (keycode != 0) {
                    uint8_t keycode_arr[6] = {keycode, 0, 0, 0, 0, 0};
                    tud_hid_keyboard_report(1, modifier, keycode_arr);
                    key_pressed = true;
                    typing_next_ms = board_millis() + 2;
                } else {
                    typing_index++;
                }
            }
        } else if (cmd_state == 2) {
            if (key_pressed) {
                tud_hid_keyboard_report(1, 0, NULL);
                key_pressed = false;
                is_typing_cmd = false; // Done
            } else {
                uint8_t keycode_arr[6] = {HID_KEY_ENTER, 0, 0, 0, 0, 0};
                tud_hid_keyboard_report(1, 0, keycode_arr);
                key_pressed = true;
                typing_next_ms = board_millis() + 50;
            }
        }
        return;
    }

    if (is_typing) {
        if (key_pressed) {
            // Release key
            tud_hid_keyboard_report(1, 0, NULL);
            key_pressed = false;
            typing_next_ms = board_millis() + 10;
            typing_index++;
        } else {
            char c = typing_buffer[typing_index];
            if (c == '\0') {
                is_typing = false;
                return;
            }
            
            uint8_t modifier = 0;
            uint8_t keycode = char_to_keycode(c, &modifier);
            
            if (keycode != 0) {
                uint8_t keycode_arr[6] = {keycode, 0, 0, 0, 0, 0};
                tud_hid_keyboard_report(1, modifier, keycode_arr);
                key_pressed = true;
                typing_next_ms = board_millis() + 10;
            } else {
                // Skip unknown chars
                typing_index++;
            }
        }
    }
}

// ============================================================
// Main
// ============================================================
int main(void)
{
    // Initialize the board for TinyUSB compatibility
    board_init();

    // Configure Button (GP0) to check for USB MSC boot mode
    gpio_init(0);
    gpio_set_dir(0, GPIO_IN);
    gpio_pull_up(0);
    
    // Short delay to allow pull-up to settle
    for (volatile int i = 0; i < 50000; i++);

    if (gpio_get(0) == 0) {
        // Button is held down -> Enter USB Mass Storage Mode
        g_msc_enabled = true;
        tud_init(BOARD_TUD_RHPORT);
        while (1) {
            tud_task();
        }
    }

    // Initialize USB for HID Simulation (MSC will be disabled via g_msc_enabled=false)
    tud_init(BOARD_TUD_RHPORT);

    // Normal Boot Mode (E-Ink Reader)
    UI_DateTime now;
    UI_LastRead last;
    uint32_t tick = board_millis();
    uint32_t last_clk = tick;

    stdio_init_all(); sleep_ms(2000);
    printf("\n=== E-Ink Reader RP2040 ===\n");
    printf("[BTN] SHORT=ok  DOUBLE=next  LONG=back\n");

    DEV_Module_Init();
    btn_init();

    // SD Card init
    // Enable internal pull-ups for SDIO to support high-capacity SD cards 
    // reliably when using jumper wires without external pull-ups.
    gpio_pull_up(18); // CMD
    gpio_pull_up(19); // D0
    gpio_pull_up(20); // D1
    gpio_pull_up(21); // D2
    gpio_pull_up(22); // D3

    static FATFS fs;
    FRESULT fr = f_mount(&fs, "0:", 1);
    
    extern void msc_flash_set_bat_code(const char* code);
    
    if (fr == FR_OK) {
        DWORD fre_clust;
        FATFS *fs_ptr;
        f_getfree("0:", &fre_clust, &fs_ptr);
        float cap = ((float)fre_clust * fs_ptr->csize * 512) / (1024.0f*1024.0f*1024.0f);
        UI_SetSdStatus(1, cap);

        // Dynamic scanning is handled by ui.c in PAGE_FILE_BROWSER
        
        // Setup Virtual Flash Drive files from pc-bat/ directory
        DIR dir;
        extern void msc_flash_clear(void);
        msc_flash_clear();
        
        if (f_opendir(&dir, "0:pc-bat") == FR_OK) {
            FILINFO fno;
            while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                if (fno.fattrib & AM_DIR) continue;
                
                // Only process .json files
                char* ext = strrchr(fno.fname, '.');
                if (ext && (strcasecmp(ext, ".json") == 0)) {
                    char filepath[256];
                    snprintf(filepath, sizeof(filepath), "0:pc-bat/%s", fno.fname);
                    
                    FIL fil;
                    if (f_open(&fil, filepath, FA_READ) == FR_OK) {
                        UINT br;
                        static char buf[1024]; // Large enough for json
                        f_read(&fil, buf, sizeof(buf)-1, &br);
                        buf[br] = '\0';
                        f_close(&fil);
                        
                        extern void msc_flash_add_file(const char* filename, const char* code);
                        msc_flash_add_file(fno.fname, buf);
                    }
                }
            }
            f_closedir(&dir);
        } else {
            // Fallback: create default time sync bat if pc-bat folder doesn't exist
            extern void msc_flash_add_file(const char* filename, const char* code);
            msc_flash_add_file("sync.json", NULL); // It will use default code
        }
        
    } else {
        UI_SetSdStatus(0, 0.0f);
        extern void msc_flash_add_file(const char* filename, const char* code);
        msc_flash_add_file("sync.json", NULL);
    }

    Platform_LoadLastRead(&last);
    Platform_ReadRtc(&now, tick);
    UI_Init(&now, &last);

    // Start background timer to poll button
    struct repeating_timer btn_timer;
    add_repeating_timer_ms(-TICK_MS, btn_timer_callback, NULL, &btn_timer);

    for (;;) {
        tud_task();
        hid_task();
        
        UI_ButtonEvent ev = pop_event();

        if (ev != UI_BUTTON_NONE) {
            UI_HandleButton(ev);
        }

        // CDC Input Polling for PC Sync
        int c = getchar_timeout_us(0);
        while (c != PICO_ERROR_TIMEOUT) {
            static char cdc_buf[64];
            static int cdc_idx = 0;
            
            if (c == '\n' || c == '\r') {
                cdc_buf[cdc_idx] = '\0';
                if (strncmp(cdc_buf, "TIME:", 5) == 0) {
                    // Expected format: TIME:2026,06,07,19,59,00
                    int y, m, d, h, min, s;
                    if (sscanf(cdc_buf + 5, "%d,%d,%d,%d,%d,%d", &y, &m, &d, &h, &min, &s) == 6) {
                        now.year = y; now.month = m; now.day = d;
                        now.hour = h; now.minute = min;
                        
                        // Update the base time for RTC calculation
                        g_base_time = now;
                        g_base_second = (uint8_t)s;
                        rtc_offset_ms = tick;
                        
                        // Approximate weekday or ignore
                        UI_UpdateClock(&now);
                        // Force a re-render if we are in main page
                        last_clk = tick; // Reset minute clock sync
                        UI_ForceSave(); // Persist to settings.json
                        UI_ReturnToMain(); // Jump back to main page and update screen
                    }
                }
                cdc_idx = 0;
            } else if (cdc_idx < 63) {
                cdc_buf[cdc_idx++] = (char)c;
            }
            c = getchar_timeout_us(0);
        }

        // Minute clock
        if ((uint32_t)(tick - last_clk) >= 1000UL) {
            Platform_ReadRtc(&now, tick);
            UI_UpdateClock(&now);
            last_clk = tick;
        }

        sleep_ms(TICK_MS);
        tick = board_millis();
    }
}
