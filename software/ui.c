#include "ui.h"
#include "EPD_2in9.h"
#include "font_gb2312.h"
#include "ff.h"
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#define UI_W 296
#define UI_H 128
#define EPD_PHYS_W 128
#define EPD_PHYS_H 296
#define EPD_ROW_BYTES (EPD_PHYS_W / 8)
#define FRAME_BYTES (EPD_ROW_BYTES * EPD_PHYS_H)

#define TIME_AREA_X 216
#define TIME_AREA_Y 0
#define TIME_AREA_W 80
#define TIME_AREA_H 32

#define PARTIALS_BEFORE_FULL 5

typedef enum {
    PAGE_MAIN = 0,
    PAGE_SETTINGS,
    PAGE_TOC,
    PAGE_READING,
    PAGE_FILE_BROWSER,
    PAGE_RESUME_PROMPT
} UI_Page;

#define MAX_BROWSER_ITEMS 50
typedef struct {
    char name[256];
    uint8_t is_dir;
} UI_FileItem;

static UI_FileItem browser_items[MAX_BROWSER_ITEMS];
static int browser_item_count = 0;
static int browser_index = 0;

enum { FILTER_NONE = 0, FILTER_TXT, FILTER_JSON, FILTER_JSON_CMD };
static int file_filter_mode = FILTER_NONE;
static uint8_t settings_index = 0;

static int ends_with_txt(const char *name) {
    int len = strlen(name);
    if (len < 4) return 0;
    const char *ext = name + len - 4;
    return (ext[0] == '.' && 
            (ext[1] == 't' || ext[1] == 'T') && 
            (ext[2] == 'x' || ext[2] == 'X') && 
            (ext[3] == 't' || ext[3] == 'T'));
}

static int ends_with_json(const char *name) {
    int len = strlen(name);
    if (len < 5) return 0;
    const char *ext = name + len - 5;
    return (ext[0] == '.' && 
            (ext[1] == 'j' || ext[1] == 'J') && 
            (ext[2] == 's' || ext[2] == 'S') && 
            (ext[3] == 'o' || ext[3] == 'O') &&
            (ext[4] == 'n' || ext[4] == 'N'));
}

static char current_dir_path[256] = "0:";

typedef struct {
    char ch;
    uint8_t row[16];
} Glyph8x16;


static uint8_t frame[FRAME_BYTES];
static UI_DateTime current_time;
static UI_LastRead last_read;
static UI_Page current_page = PAGE_MAIN;
static uint16_t toc_highlight = 1;
static uint8_t partial_count = 0;
static int sd_mounted = 0;
static float sd_capacity = 0.0f;

// Platform hooks. Override these in another C file to connect flash/NVS.
__attribute__((weak)) void UI_SaveSettingsToStorage(uint8_t setting_index)
{
    (void)setting_index;
}

__attribute__((weak)) void UI_SaveLastReadToStorage(const UI_LastRead *last)
{
    (void)last;
}



static const Glyph8x16 FONT[] = {
    {' ',{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
    {'!',{0,0,24,60,60,60,24,24,24,0,24,24,0,0,0,0}},
    {'-',{0,0,0,0,0,0,0,254,0,0,0,0,0,0,0,0}},
    {'.',{0,0,0,0,0,0,0,0,0,0,24,24,0,0,0,0}},
    {'/',{0,0,0,0,2,6,12,24,48,96,192,128,0,0,0,0}},
    {':',{0,0,0,0,24,24,0,0,0,24,24,0,0,0,0,0}},
    {'?',{0,0,124,198,198,12,24,24,24,0,24,24,0,0,0,0}},
    {'0',{0,0,60,102,195,195,219,219,195,195,102,60,0,0,0,0}},
    {'1',{0,0,24,56,120,24,24,24,24,24,24,126,0,0,0,0}},
    {'2',{0,0,124,198,6,12,24,48,96,192,198,254,0,0,0,0}},
    {'3',{0,0,124,198,6,6,60,6,6,6,198,124,0,0,0,0}},
    {'4',{0,0,12,28,60,108,204,254,12,12,12,30,0,0,0,0}},
    {'5',{0,0,254,192,192,192,252,6,6,6,198,124,0,0,0,0}},
    {'6',{0,0,56,96,192,192,252,198,198,198,198,124,0,0,0,0}},
    {'7',{0,0,254,198,6,6,12,24,48,48,48,48,0,0,0,0}},
    {'8',{0,0,124,198,198,198,124,198,198,198,198,124,0,0,0,0}},
    {'9',{0,0,124,198,198,198,126,6,6,6,12,120,0,0,0,0}},
    {'A',{0,0,16,56,108,198,198,254,198,198,198,198,0,0,0,0}},
    {'B',{0,0,252,102,102,102,124,102,102,102,102,252,0,0,0,0}},
    {'C',{0,0,60,102,194,192,192,192,192,194,102,60,0,0,0,0}},
    {'D',{0,0,248,108,102,102,102,102,102,102,108,248,0,0,0,0}},
    {'E',{0,0,254,102,98,104,120,104,96,98,102,254,0,0,0,0}},
    {'F',{0,0,254,102,98,104,120,104,96,96,96,240,0,0,0,0}},
    {'G',{0,0,60,102,194,192,192,222,198,198,102,58,0,0,0,0}},
    {'H',{0,0,198,198,198,198,254,198,198,198,198,198,0,0,0,0}},
    {'I',{0,0,60,24,24,24,24,24,24,24,24,60,0,0,0,0}},
    {'J',{0,0,30,12,12,12,12,12,204,204,204,120,0,0,0,0}},
    {'K',{0,0,230,102,102,108,120,120,108,102,102,230,0,0,0,0}},
    {'L',{0,0,240,96,96,96,96,96,96,98,102,254,0,0,0,0}},
    {'M',{0,0,195,231,255,255,219,195,195,195,195,195,0,0,0,0}},
    {'N',{0,0,198,230,246,254,222,206,198,198,198,198,0,0,0,0}},
    {'O',{0,0,124,198,198,198,198,198,198,198,198,124,0,0,0,0}},
    {'P',{0,0,252,102,102,102,124,96,96,96,96,240,0,0,0,0}},
    {'Q',{0,0,124,198,198,198,198,198,198,214,222,124,12,14,0,0}},
    {'R',{0,0,252,102,102,102,124,108,102,102,102,230,0,0,0,0}},
    {'S',{0,0,124,198,198,96,56,12,6,198,198,124,0,0,0,0}},
    {'T',{0,0,255,219,153,24,24,24,24,24,24,60,0,0,0,0}},
    {'U',{0,0,198,198,198,198,198,198,198,198,198,124,0,0,0,0}},
    {'V',{0,0,195,195,195,195,195,195,195,102,60,24,0,0,0,0}},
    {'W',{0,0,195,195,195,195,195,219,219,255,102,102,0,0,0,0}},
    {'X',{0,0,195,195,102,60,24,24,60,102,195,195,0,0,0,0}},
    {'Y',{0,0,195,195,195,102,60,24,24,24,24,60,0,0,0,0}},
    {'Z',{0,0,255,195,134,12,24,48,96,193,195,255,0,0,0,0}},
    {'a',{0,0,0,0,0,120,12,124,204,204,204,118,0,0,0,0}},
    {'b',{0,0,224,96,96,120,108,102,102,102,102,124,0,0,0,0}},
    {'c',{0,0,0,0,0,124,198,192,192,192,198,124,0,0,0,0}},
    {'d',{0,0,28,12,12,60,108,204,204,204,204,118,0,0,0,0}},
    {'e',{0,0,0,0,0,124,198,254,192,192,198,124,0,0,0,0}},
    {'f',{0,0,56,108,100,96,240,96,96,96,96,240,0,0,0,0}},
    {'g',{0,0,0,0,0,118,204,204,204,204,204,124,12,204,120,0}},
    {'h',{0,0,224,96,96,108,118,102,102,102,102,230,0,0,0,0}},
    {'i',{0,0,24,24,0,56,24,24,24,24,24,60,0,0,0,0}},
    {'j',{0,0,6,6,0,14,6,6,6,6,6,6,102,102,60,0}},
    {'k',{0,0,224,96,96,102,108,120,120,108,102,230,0,0,0,0}},
    {'l',{0,0,56,24,24,24,24,24,24,24,24,60,0,0,0,0}},
    {'m',{0,0,0,0,0,230,255,219,219,219,219,219,0,0,0,0}},
    {'n',{0,0,0,0,0,220,102,102,102,102,102,102,0,0,0,0}},
    {'o',{0,0,0,0,0,124,198,198,198,198,198,124,0,0,0,0}},
    {'p',{0,0,0,0,0,220,102,102,102,102,102,124,96,96,240,0}},
    {'q',{0,0,0,0,0,118,204,204,204,204,204,124,12,12,30,0}},
    {'r',{0,0,0,0,0,220,118,102,96,96,96,240,0,0,0,0}},
    {'s',{0,0,0,0,0,124,198,96,56,12,198,124,0,0,0,0}},
    {'t',{0,0,16,48,48,252,48,48,48,48,54,28,0,0,0,0}},
    {'u',{0,0,0,0,0,204,204,204,204,204,204,118,0,0,0,0}},
    {'v',{0,0,0,0,0,195,195,195,195,102,60,24,0,0,0,0}},
    {'w',{0,0,0,0,0,195,195,195,219,219,255,102,0,0,0,0}},
    {'x',{0,0,0,0,0,195,102,60,24,60,102,195,0,0,0,0}},
    {'y',{0,0,0,0,0,198,198,198,198,198,198,126,6,12,248,0}},
    {'z',{0,0,0,0,0,254,204,24,48,96,198,254,0,0,0,0}},
};

static const uint8_t *font_lookup(char ch)
{
    for(int i = 0; i < (int)(sizeof(FONT) / sizeof(FONT[0])); i++) {
        if(FONT[i].ch == ch) return FONT[i].row;
    }
    return FONT[6].row; // '?'
}

static void frame_clear(void)
{
    for(int i = 0; i < FRAME_BYTES; i++) frame[i] = 0xFF;
}

static void put_pixel(int lx, int ly, int black)
{
    if(lx < 0 || lx >= UI_W || ly < 0 || ly >= UI_H) return;

    // Rotate logical landscape into the 128x296 physical panel buffer.
    int px = ly;
    int py = (EPD_PHYS_H - 1) - lx;
    int bi = (px >> 3) + py * EPD_ROW_BYTES;
    int bit = 7 - (px & 0x07);

    if(black) frame[bi] &= (uint8_t)~(1 << bit);
    else      frame[bi] |= (uint8_t) (1 << bit);
}

static void fill_rect(int lx, int ly, int w, int h, int black)
{
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) put_pixel(lx + x, ly + y, black);
    }
}

static void hline(int lx, int ly, int w, int black)
{
    for(int x = 0; x < w; x++) put_pixel(lx + x, ly, black);
}



static void draw_glyph(int lx, int ly, const uint8_t *g, int ink_black, int bold)
{
    if (!g) return;
    for(int row = 0; row < 16; row++) {
        uint8_t bits1 = g[row * 2];
        uint8_t bits2 = g[row * 2 + 1];
        
        for(int col = 0; col < 8; col++) {
            if(bits1 & (0x80 >> col)) {
                put_pixel(lx + col, ly + row, ink_black);
                if(bold && col < 7) put_pixel(lx + col + 1, ly + row, ink_black);
            }
        }
        for(int col = 0; col < 8; col++) {
            if(bits2 & (0x80 >> col)) {
                put_pixel(lx + 8 + col, ly + row, ink_black);
                if(bold && col < 7) put_pixel(lx + 8 + col + 1, ly + row, ink_black);
            }
        }
    }
}

static uint16_t decode_utf8(const char **s) {
    uint32_t c = (unsigned char)**s;
    if (c == 0) return 0;
    (*s)++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) {
        c = (c & 0x1F) << 6;
        c |= ((unsigned char)**s & 0x3F);
        (*s)++;
        return c;
    }
    if ((c & 0xF0) == 0xE0) {
        c = (c & 0x0F) << 12;
        c |= (((unsigned char)**s & 0x3F) << 6);
        (*s)++;
        c |= ((unsigned char)**s & 0x3F);
        (*s)++;
        return c;
    }
    if ((c & 0xF8) == 0xF0) {
        (*s) += 3;
        return '?';
    }
    return '?';
}

static void draw_char(int lx, int ly, char ch, int ink_black, int bold)
{
    int left = 0;
    int right = sizeof(FONT) / sizeof(Glyph8x16) - 1;
    const uint8_t *g = NULL;

    while(left <= right) {
        int mid = left + (right - left) / 2;
        if(FONT[mid].ch == ch) {
            g = FONT[mid].row;
            break;
        }
        if(FONT[mid].ch < ch) left = mid + 1;
        else right = mid - 1;
    }
    if (!g) g = FONT[0].row; // default to ' '

    for(int row = 0; row < 16; row++) {
        uint8_t bits = g[row];
        for(int col = 0; col < 8; col++) {
            if(bits & (0x80 >> col)) {
                put_pixel(lx + col, ly + row, ink_black);
                if(bold && col < 7) put_pixel(lx + col + 1, ly + row, ink_black);
            }
        }
    }
}

static void draw_text_ex(int lx, int ly, const char *s, int ink_black, int bold)
{
    while(s && *s) {
        uint16_t unicode = decode_utf8(&s);
        if (unicode == 0) break;
        
        if (unicode < 0x80) {
            draw_char(lx, ly, (char)unicode, ink_black, bold);
            lx += 8;
        } else {
            const uint8_t *g = font_get_glyph(unicode);
            if (!g) g = font_get_glyph('?');
            draw_glyph(lx, ly, g, ink_black, bold);
            lx += 16;
        }
    }
}

static void draw_text(int lx, int ly, const char *s, int bold)
{
    draw_text_ex(lx, ly, s, 1, bold);
}

static uint16_t next_display_cell(const char **ps)
{
    return decode_utf8(ps);
}

static int display_cell_count(const char *s)
{
    int n = 0;
    const char *p = s;

    while(p && *p) {
        uint16_t u = next_display_cell(&p);
        n += (u < 0x80) ? 1 : 2;
    }
    return n;
}

static void draw_ellipsis_cell(int lx, int ly, int black)
{
    fill_rect(lx + 1, ly + 12, 2, 2, black);
    fill_rect(lx + 4, ly + 12, 2, 2, black);
    fill_rect(lx + 7, ly + 12, 2, 2, black);
}

static void draw_text_max_cells(int lx, int ly, const char *s, int max_cells, int bold)
{
    int cells = display_cell_count(s);
    int limit = cells > max_cells ? max_cells - 2 : max_cells; // -2 for ellipsis if needed
    const char *p = s;
    int current_cells = 0;

    while (p && *p && current_cells < limit) {
        uint16_t u = next_display_cell(&p);
        int w = (u < 0x80) ? 1 : 2;
        if (current_cells + w > limit) break;
        
        if (u < 0x80) {
            draw_char(lx + current_cells * 8, ly, (char)u, 1, bold);
        } else {
            const uint8_t *g = font_get_glyph(u);
            if (!g) g = font_get_glyph('?');
            draw_glyph(lx + current_cells * 8, ly, g, 1, bold);
        }
        current_cells += w;
    }

    if(cells > max_cells) draw_ellipsis_cell(lx + current_cells * 8, ly, 1);
}

static void append_char(char *buf, int *p, int max, char c)
{
    if(*p < max - 1) buf[(*p)++] = c;
    buf[*p] = 0;
}

static void append_str(char *buf, int *p, int max, const char *s)
{
    while(s && *s) append_char(buf, p, max, *s++);
}

static void append_uint(char *buf, int *p, int max, uint16_t v)
{
    char tmp[6];
    int n = 0;

    if(v == 0) {
        append_char(buf, p, max, '0');
        return;
    }

    while(v && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }

    while(n > 0) append_char(buf, p, max, tmp[--n]);
}

static void append_2d(char *buf, int *p, int max, uint8_t v)
{
    append_char(buf, p, max, (char)('0' + (v / 10) % 10));
    append_char(buf, p, max, (char)('0' + v % 10));
}

static void append_4d(char *buf, int *p, int max, uint16_t v)
{
    append_char(buf, p, max, (char)('0' + (v / 1000) % 10));
    append_char(buf, p, max, (char)('0' + (v / 100) % 10));
    append_char(buf, p, max, (char)('0' + (v / 10) % 10));
    append_char(buf, p, max, (char)('0' + v % 10));
}

static void copy_text(char *dst, int dst_len, const char *src)
{
    int i = 0;

    if(!src) src = "";
    while(i < dst_len - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void copy_last_read(const UI_LastRead *last)
{
    if(!last) return;

    copy_text(last_read.novel, (int)sizeof(last_read.novel), last->novel);
    last_read.chapter = last->chapter ? last->chapter : 1;
    last_read.chapter_count = last->chapter_count ? last->chapter_count : 1;
    last_read.page = last->page ? last->page : 1;
}

static int same_date(const UI_DateTime *a, const UI_DateTime *b)
{
    return a->year == b->year && a->month == b->month && a->day == b->day;
}

static int same_minute(const UI_DateTime *a, const UI_DateTime *b)
{
    return same_date(a, b) && a->hour == b->hour && a->minute == b->minute;
}

static void make_date(char *buf, int max, const UI_DateTime *t)
{
    static const char *wd[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    int p = 0;

    append_4d(buf, &p, max, t->year);
    append_char(buf, &p, max, '-');
    append_2d(buf, &p, max, t->month);
    append_char(buf, &p, max, '-');
    append_2d(buf, &p, max, t->day);
    append_char(buf, &p, max, ' ');
    append_str(buf, &p, max, wd[t->weekday % 7]);
}

static void make_progress(char *buf, int max)
{
    int p = 0;
    if (ends_with_txt(last_read.novel)) {
        append_str(buf, &p, max, "Pg. ");
        append_uint(buf, &p, max, last_read.page);
        
        char idx_file[256];
        strncpy(idx_file, last_read.novel, 255);
        idx_file[255] = '\0';
        int len = strlen(idx_file);
        if (len >= 4) {
            strcpy(idx_file + len - 4, ".idx");
            FIL fp_idx;
            if (f_open(&fp_idx, idx_file, FA_READ) == FR_OK) {
                uint32_t tot = f_size(&fp_idx) / 4;
                f_close(&fp_idx);
                append_str(buf, &p, max, " / ");
                append_uint(buf, &p, max, tot);
            }
        }
    } else {
        append_str(buf, &p, max, "Ch. ");
        append_uint(buf, &p, max, last_read.chapter);
        append_str(buf, &p, max, " / ");
        append_uint(buf, &p, max, last_read.chapter_count);
    }
}


// 24px bold, pure black/white, seven-segment time digits.
static void draw_digit_24(int lx, int ly, uint8_t digit)
{
    static const uint8_t segs[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    uint8_t s = segs[digit % 10];

    if(s & 0x01) fill_rect(lx + 2,  ly,      9, 3, 1);  // A
    if(s & 0x02) fill_rect(lx + 10, ly + 2,  3, 9, 1);  // B
    if(s & 0x04) fill_rect(lx + 10, ly + 13, 3, 9, 1);  // C
    if(s & 0x08) fill_rect(lx + 2,  ly + 21, 9, 3, 1);  // D
    if(s & 0x10) fill_rect(lx,      ly + 13, 3, 9, 1);  // E
    if(s & 0x20) fill_rect(lx,      ly + 2,  3, 9, 1);  // F
    if(s & 0x40) fill_rect(lx + 2,  ly + 11, 9, 3, 1);  // G
}

static void draw_colon_24(int lx, int ly)
{
    fill_rect(lx + 1, ly + 7,  3, 3, 1);
    fill_rect(lx + 1, ly + 16, 3, 3, 1);
}

static void draw_time_24(int lx, int ly, const UI_DateTime *t)
{
    draw_digit_24(lx,      ly, t->hour / 10);
    draw_digit_24(lx + 15, ly, t->hour % 10);
    draw_colon_24(lx + 30, ly);
    draw_digit_24(lx + 37, ly, t->minute / 10);
    draw_digit_24(lx + 52, ly, t->minute % 10);
}

static void draw_top_bar(void)
{
    char date[20];

    make_date(date, (int)sizeof(date), &current_time);
    
    // Pixel art style inverted top bar
    fill_rect(0, 0, UI_W, 32, 1);
    draw_text_ex(8, 8, date, 0, 1);
    
    // Time box
    fill_rect(216, 2, 76, 28, 0);
    draw_time_24(224, 4, &current_time);
    
    // Separator line
    hline(0, 32, UI_W, 0);
    hline(0, 33, UI_W, 1);
    hline(0, 34, UI_W, 1);
}

static void display_full(void)
{
    // Full refresh on every large page switch keeps the panel's black/white
    // reference clean. The panel sleeps afterward to cut static power.
    EPD_2IN9_Init(EPD_2IN9_FULL);
    EPD_2IN9_DisplayWithBG(frame, 0, 0, 0);
    EPD_2IN9_Sleep();
    partial_count = 0;
}

static void display_partial_landscape(int lx, int ly, int w, int h)
{
    (void)lx;
    (void)ly;
    (void)w;
    (void)h;
    
    if (partial_count >= PARTIALS_BEFORE_FULL) {
        display_full();
    } else {
        EPD_2IN9_Init(EPD_2IN9_PART);
        EPD_2IN9_DisplayWithBG(frame, 0, 0, 0);
        EPD_2IN9_Sleep();
        partial_count++;
    }
}

static void render_header(const char *title)
{
    frame_clear();
    
    // Extract filename from path to keep header clean
    const char *display_name = strrchr(title, '/');
    if (display_name) display_name++;
    else display_name = title;

    // Truncate to 20 cells (160 pixels max) to avoid colliding with progress text at X=180
    draw_text_max_cells(8, 8, display_name, 20, 1);
    hline(8, 32, 280, 1);
}

static void render_main(void)
{
    char progress[20];

    frame_clear();
    draw_top_bar();

    // Pixel art retro window
    // Window outline
    fill_rect(8, 44, 280, 76, 1);
    // Window inner
    fill_rect(10, 46, 276, 72, 0);

    // Inner title bar
    fill_rect(10, 46, 276, 18, 1);
    draw_text_ex(14, 47, "READING PROGRESS", 0, 1);

    // Book Icon (Pixel art)
    fill_rect(18, 72, 28, 32, 1);
    fill_rect(20, 74, 24, 28, 0);
    fill_rect(22, 74, 2, 28, 1); // spine line
    // pages
    fill_rect(26, 78, 16, 2, 1);
    fill_rect(26, 84, 16, 2, 1);
    fill_rect(26, 90, 12, 2, 1);
    fill_rect(26, 96, 16, 2, 1);

    // Book title
    const char *display_name = strrchr(last_read.novel, '/');
    if (display_name) display_name++; // skip '/'
    else display_name = last_read.novel;
    if (display_name[0] == '\0') display_name = "未选择";
    draw_text_max_cells(56, 72, display_name, 26, 1);
    
    // SD Card Status Box
    int sd_box_x = 140;
    int sd_box_y = 108;
    int sd_box_w = 130;
    int sd_box_h = 16;
    fill_rect(sd_box_x, sd_box_y, sd_box_w, sd_box_h, 1);
    fill_rect(sd_box_x + 1, sd_box_y + 1, sd_box_w - 2, sd_box_h - 2, 0);

    if (sd_mounted) {
        char sd_info[32];
        snprintf(sd_info, sizeof(sd_info), "SD卡: %.1f GB", sd_capacity);
        draw_text_ex(sd_box_x + 4, sd_box_y + 1, sd_info, 1, 0);
    } else {
        draw_text_ex(sd_box_x + 4, sd_box_y + 1, "无SD卡", 1, 0);
    }

    if(last_read.novel[0] != '\0') {
        make_progress(progress, (int)sizeof(progress));
        draw_text(56, 92, progress, 0);
        
        // Progress bar
        int bar_x = 140;
        int bar_y = 94;
        int bar_w = 130;
        int bar_h = 12;
        fill_rect(bar_x, bar_y, bar_w, bar_h, 1);
        fill_rect(bar_x + 2, bar_y + 2, bar_w - 4, bar_h - 4, 0);
        
        int fill_w = 0;
        if (ends_with_txt(last_read.novel)) {
            char idx_file[256];
            strncpy(idx_file, last_read.novel, 255);
            idx_file[255] = '\0';
            int len = strlen(idx_file);
            if (len >= 4) {
                strcpy(idx_file + len - 4, ".idx");
                FIL fp_idx;
                if (f_open(&fp_idx, idx_file, FA_READ) == FR_OK) {
                    uint32_t tot = f_size(&fp_idx) / 4;
                    f_close(&fp_idx);
                    if (tot > 0) {
                        fill_w = ((bar_w - 4) * last_read.page) / tot;
                    }
                }
            }
        } else {
            if (last_read.chapter_count > 0) {
                fill_w = ((bar_w - 4) * last_read.chapter) / last_read.chapter_count;
            }
        }
        
        if (fill_w > bar_w - 4) fill_w = bar_w - 4;
        
        if (fill_w > 0) {
            // Dotted fill for pixel style
            for (int px = 0; px < fill_w; px += 2) {
                fill_rect(bar_x + 2 + px, bar_y + 2, 1, bar_h - 4, 1);
            }
        }
    }
}

static void scan_directory(const char *path) {
    DIR dir;
    FILINFO fno;
    browser_item_count = 0;
    browser_index = 0;

    if (f_opendir(&dir, path) == FR_OK) {
        int dir_count = 0;
        static UI_FileItem temp_dirs[MAX_BROWSER_ITEMS];
        int file_count = 0;
        static UI_FileItem temp_files[MAX_BROWSER_ITEMS];

        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
            if (fno.fattrib & (AM_HID | AM_SYS)) continue;
            
            if (file_filter_mode == FILTER_TXT) {
                if ((fno.fattrib & AM_DIR) || !ends_with_txt(fno.fname)) {
                    continue;
                }
            } else if (file_filter_mode == FILTER_JSON || file_filter_mode == FILTER_JSON_CMD) {
                if ((fno.fattrib & AM_DIR) || !ends_with_json(fno.fname)) {
                    continue;
                }
            }
            
            if (fno.fattrib & AM_DIR) {
                if (dir_count < MAX_BROWSER_ITEMS) {
                    strncpy(temp_dirs[dir_count].name, fno.fname, 255);
                    temp_dirs[dir_count].name[255] = '\0';
                    temp_dirs[dir_count].is_dir = 1;
                    dir_count++;
                }
            } else {
                if (file_count < MAX_BROWSER_ITEMS) {
                    strncpy(temp_files[file_count].name, fno.fname, 255);
                    temp_files[file_count].name[255] = '\0';
                    temp_files[file_count].is_dir = 0;
                    file_count++;
                }
            }
        }
        f_closedir(&dir);

        for(int i=0; i<dir_count && browser_item_count < MAX_BROWSER_ITEMS; i++) {
            browser_items[browser_item_count++] = temp_dirs[i];
        }
        for(int i=0; i<file_count && browser_item_count < MAX_BROWSER_ITEMS; i++) {
            browser_items[browser_item_count++] = temp_files[i];
        }
    }
}

static const char *settings_menu[] = {
    "SD卡",
    "模拟键盘",
    "同步电脑数据"
};
#define SETTINGS_MENU_COUNT 3

static void render_settings(void)
{
    render_header("Settings");

    if (!sd_mounted) {
        draw_text(16, 48, "无SD卡", 0);
        return;
    }

    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        int display_y = 48 + i * 26;
        char item_text[40];
        
        if (i == settings_index) {
            fill_rect(8, display_y - 2, 280, 22, 1);
            snprintf(item_text, sizeof(item_text), "> %s", settings_menu[i]);
            draw_text_ex(16, display_y, item_text, 0, 1);
        } else {
            snprintf(item_text, sizeof(item_text), "  %s", settings_menu[i]);
            draw_text(16, display_y, item_text, 0);
        }
    }
}

static void render_file_browser(void)
{
    render_header(current_dir_path);

    if (browser_item_count == 0) {
        draw_text(16, 48, "文件夹为空", 0);
        return;
    }

    int page = browser_index / 3;
    int start_idx = page * 3;
    int end_idx = start_idx + 3;
    if (end_idx > browser_item_count) end_idx = browser_item_count;

    for (int i = start_idx; i < end_idx; i++) {
        int display_y = 48 + (i - start_idx) * 26;
        char item_text[40];
        
        if (i == browser_index) {
            fill_rect(8, display_y - 2, 280, 22, 1);
            snprintf(item_text, sizeof(item_text), "> %s%s", browser_items[i].name, browser_items[i].is_dir ? "/" : "");
            draw_text_ex(16, display_y, item_text, 0, 1);
        } else {
            snprintf(item_text, sizeof(item_text), "  %s%s", browser_items[i].name, browser_items[i].is_dir ? "/" : "");
            draw_text(16, display_y, item_text, 0);
        }
    }
}

static void render_toc(void)
{
    render_header("Table of Contents");
    draw_text(16, 48, "TXT不支持目录", 0);
}

#define MAX_TXT_PAGES 2000
static uint32_t txt_page_offsets[MAX_TXT_PAGES];
static uint16_t txt_total_pages = 0;
static char txt_current_file[256] = "";

static uint8_t txt_has_idx = 0;
static uint32_t txt_total_pages_idx = 0;

static void init_txt_reading(const char *filename)
{
    if (strcmp(txt_current_file, filename) != 0) {
        strncpy(txt_current_file, filename, 255);
        txt_current_file[255] = '\0';
        txt_page_offsets[0] = 0;
        txt_total_pages = 1;
        txt_has_idx = 0;
        
        char idx_file[256];
        strncpy(idx_file, filename, 255);
        idx_file[255] = '\0';
        int len = strlen(idx_file);
        if (len >= 4) {
            strcpy(idx_file + len - 4, ".idx");
            FIL fp_idx;
            if (f_open(&fp_idx, idx_file, FA_READ) == FR_OK) {
                txt_has_idx = 1;
                txt_total_pages_idx = f_size(&fp_idx) / 4;
                f_close(&fp_idx);
            }
        }
    }
}

static void scan_txt_to_page(uint16_t target_page)
{
    if (!sd_mounted) return;
    if (txt_has_idx) return; // Skip scanning if .idx is present
    if (target_page <= txt_total_pages) return;
    
    FIL fp;
    if (f_open(&fp, txt_current_file, FA_READ) != FR_OK) return;
    
    uint32_t current_offset = txt_page_offsets[txt_total_pages - 1];
    f_lseek(&fp, current_offset);
    
    uint8_t buf[256];
    
    while (txt_total_pages < target_page && txt_total_pages < MAX_TXT_PAGES) {
        int y = 40;
        int x = 8;
        int buf_len = 0;
        int buf_idx = 0;
        int eof = 0;
        
        while (y <= 112) {
            if (buf_idx >= buf_len - 4 && !eof) {
                int rem = buf_len - buf_idx;
                if (rem < 0) rem = 0; // Prevent negative rem stack corruption
                if (rem > 0) memmove(buf, &buf[buf_idx], rem);
                UINT br;
                f_read(&fp, buf + rem, sizeof(buf) - rem, &br);
                buf_len = rem + br;
                buf_idx = 0;
                if (br == 0) { eof = 1; }
            }
            
            if (buf_idx >= buf_len) {
                break;
            }
            
            const char *ptr = (const char *)&buf[buf_idx];
            uint16_t u = decode_utf8(&ptr);
            int consumed = ptr - (const char *)&buf[buf_idx];
            if (consumed == 0) { buf_idx++; continue; }
            
            buf_idx += consumed;
            
            if (u == '\n') {
                y += 16;
                x = 8;
                continue;
            }
            if (u == '\r') continue;
            
            int w = (u < 0x80) ? 8 : 16;
            if (x + w > 296 - 8) {
                y += 16;
                x = 8;
                if (y > 112) {
                    uint32_t next_pos = f_tell(&fp) - buf_len + buf_idx - consumed;
                    f_lseek(&fp, next_pos);
                    break;
                }
            }
            x += w;
        }
        
        if (eof) break;
        txt_page_offsets[txt_total_pages++] = f_tell(&fp);
    }
    f_close(&fp);
}

static void render_reading(void)
{
    render_header(last_read.novel);

    if (!sd_mounted) {
        draw_text(8, 40, "无SD卡", 0);
        return;
    }

    if (last_read.novel[0] == '\0') {
        draw_text(8, 40, "未选择小说", 0);
        return;
    }

    FIL fp;
    if (f_open(&fp, last_read.novel, FA_READ) != FR_OK) {
        draw_text(8, 40, "读取小说失败", 0);
        return;
    }

    init_txt_reading(last_read.novel);
    
    if (last_read.page < 1) last_read.page = 1;
    
    uint32_t total_pages = txt_has_idx ? txt_total_pages_idx : txt_total_pages;
    
    if (!txt_has_idx) {
        scan_txt_to_page(last_read.page);
        total_pages = txt_total_pages;
    }
    
    if (last_read.page > total_pages && total_pages > 0) {
        last_read.page = total_pages;
    }

    uint32_t fsize = f_size(&fp);
    
    uint32_t start_offset = 0;
    if (txt_has_idx) {
        char idx_file[256];
        strncpy(idx_file, last_read.novel, 255);
        idx_file[255] = '\0';
        strcpy(idx_file + strlen(idx_file) - 4, ".idx");
        FIL fp_idx;
        if (f_open(&fp_idx, idx_file, FA_READ) == FR_OK) {
            f_lseek(&fp_idx, (last_read.page - 1) * 4);
            UINT br;
            f_read(&fp_idx, &start_offset, 4, &br);
            f_close(&fp_idx);
        }
    } else {
        start_offset = txt_page_offsets[last_read.page - 1];
    }

    char info[32];
    if (txt_has_idx || fsize == 0) {
        snprintf(info, sizeof(info), "Pg %d/%lu", last_read.page, (unsigned long)total_pages);
    } else {
        float pct = (float)start_offset * 100.0f / (float)fsize;
        snprintf(info, sizeof(info), "Pg %d (%.1f%%)", last_read.page, pct);
    }
    draw_text(180, 8, info, 1);

    f_lseek(&fp, start_offset);

    int y = 40;
    int x = 8;
    uint8_t buf[256];
    int buf_len = 0;
    int buf_idx = 0;
    int eof = 0;
    
    while (y <= 112) {
        if (buf_idx >= buf_len - 4 && !eof) {
            int rem = buf_len - buf_idx;
            if (rem < 0) rem = 0;
            if (rem > 0) memmove(buf, &buf[buf_idx], rem);
            UINT br;
            f_read(&fp, buf + rem, sizeof(buf) - rem, &br);
            buf_len = rem + br;
            buf_idx = 0;
            if (br == 0) { eof = 1; }
        }
        
        if (buf_idx >= buf_len) {
            break;
        }
        
        const char *ptr = (const char *)&buf[buf_idx];
        uint16_t u = decode_utf8(&ptr);
        int consumed = ptr - (const char *)&buf[buf_idx];
        if (consumed == 0) { buf_idx++; continue; }
        
        buf_idx += consumed;
        
        if (u == '\n') {
            y += 16;
            x = 8;
            continue;
        }
        if (u == '\r') continue;
        
        int w = (u < 0x80) ? 8 : 16;
        if (x + w > 296 - 8) {
            y += 16;
            x = 8;
            if (y > 112) break;
        }
        
        if (u < 0x80) {
            draw_char(x, y, (char)u, 1, 0);
        } else {
            const uint8_t *g = font_get_glyph(u);
            if (!g) g = font_get_glyph('?');
            draw_glyph(x, y, g, 1, 0);
        }
        x += w;
    }
    f_close(&fp);
}

static void render_resume_prompt(void)
{
    render_header("Resume Reading?");
    
    fill_rect(8, 40, 280, 80, 1);
    fill_rect(10, 42, 276, 76, 0);
    
    draw_text(16, 48, "检测到历史进度", 0);
    
    const char *display_name = strrchr(last_read.novel, '/');
    if (display_name) display_name++;
    else display_name = last_read.novel;
    
    draw_text_ex(16, 68, display_name, 0, 1);
    
    draw_text(16, 96, "短按: 继续    双击: 重头开始", 0);
}

static void enter_main(void)
{
    current_page = PAGE_MAIN;
    render_main();
    display_full();
}
void UI_ReturnToMain(void)
{
    current_page = PAGE_MAIN;
    render_main();
    display_full();
}

void UI_ForceSave(void)
{
    UI_SaveLastReadToStorage(&last_read);
}

static void enter_settings(void)
{
    current_page = PAGE_SETTINGS;
    render_settings();
    display_full();
}

static void enter_resume_prompt(void)
{
    current_page = PAGE_RESUME_PROMPT;
    render_resume_prompt();
    display_full();
}

static void enter_file_browser(void)
{
    current_page = PAGE_FILE_BROWSER;
    scan_directory(current_dir_path);
    render_file_browser();
    display_full();
}

static void enter_toc(void)
{
    current_page = PAGE_TOC;
    toc_highlight = last_read.chapter ? last_read.chapter : 1;
    render_toc();
    display_full();
}

static void enter_reading(void)
{
    current_page = PAGE_READING;
    last_read.chapter = toc_highlight;
    render_reading();
    display_full();
}

void UI_Init(const UI_DateTime *now, const UI_LastRead *last)
{
    if(now) current_time = *now;
    else {
        current_time.year = 2026;
        current_time.month = 6;
        current_time.day = 4;
        current_time.weekday = 4;
        current_time.hour = 10;
        current_time.minute = 30;
    }

    if (last && last->novel[0] != '\0') {
        last_read = *last;
        file_filter_mode = FILTER_NONE;
    } else {
        last_read.novel[0] = '\0';
        last_read.chapter = 1;
        last_read.chapter_count = 1;
        last_read.page = 1;
    }
    
    enter_main();
}

void UI_GetCurrentTime(UI_DateTime *out_time)
{
    if (out_time) {
        *out_time = current_time;
    }
}

void UI_UpdateClock(const UI_DateTime *now)
{
    UI_DateTime old_time;

    if(!now) return;
    if(current_page != PAGE_MAIN) {
        current_time = *now;
        return;
    }

    if(same_minute(&current_time, now)) return;

    old_time = current_time;
    current_time = *now;
    render_main();

    if(!same_date(&old_time, now)) {
        display_full();
    } else {
        // Minute changes touch only the top-right time rectangle.
        display_partial_landscape(TIME_AREA_X, TIME_AREA_Y, TIME_AREA_W, TIME_AREA_H);
    }
}

void UI_SetLastRead(const UI_LastRead *last)
{
    copy_last_read(last);
    if(current_page == PAGE_MAIN) {
        render_main();
        display_full();
    }
}

void UI_SetSdStatus(int mounted, float capacity_gb)
{
    sd_mounted = mounted;
    sd_capacity = capacity_gb;
    if (current_page == PAGE_MAIN) {
        render_main();
        display_full();
    }
}

void UI_HandleButton(UI_ButtonEvent event)
{
    if (event == UI_BUTTON_NONE) return;

    switch (current_page) {
    case PAGE_MAIN:
        if (event == UI_BUTTON_SHORT) {
            enter_settings();
        } else if (event == UI_BUTTON_DOUBLE) {
            file_filter_mode = FILTER_TXT;
            strcpy(current_dir_path, "0:");
            enter_file_browser();
        }
        break;

    case PAGE_SETTINGS:
        if (event == UI_BUTTON_SHORT) {
            if (sd_mounted) {
                if (settings_index == 0) {
                    file_filter_mode = FILTER_NONE;
                    strcpy(current_dir_path, "0:");
                    enter_file_browser();
                } else if (settings_index == 1) {
                    file_filter_mode = FILTER_JSON;
                    strcpy(current_dir_path, "0:/simulation");
                    // Ensure directory exists or ignore
                    enter_file_browser();
                } else if (settings_index == 2) {
                    file_filter_mode = FILTER_JSON_CMD;
                    strcpy(current_dir_path, "0:/pc_cmd");
                    enter_file_browser();
                }
            }
        } else if (event == UI_BUTTON_DOUBLE) {
            settings_index = (settings_index + 1) % SETTINGS_MENU_COUNT;
            render_settings();
            display_partial_landscape(0, 32, UI_W, UI_H - 32);
        } else if (event == UI_BUTTON_LONG) {
            enter_main();
        }
        break;

    case PAGE_FILE_BROWSER:
        if (event == UI_BUTTON_SHORT) {
            if (browser_item_count > 0 && browser_index < browser_item_count) {
                UI_FileItem *item = &browser_items[browser_index];
                if (item->is_dir) {
                    if (strcmp(current_dir_path, "0:") == 0) {
                        snprintf(current_dir_path, sizeof(current_dir_path), "0:/%s", item->name);
                    } else {
                        int len = strlen(current_dir_path);
                        snprintf(current_dir_path + len, sizeof(current_dir_path) - len, "/%s", item->name);
                    }
                    enter_file_browser();
                } else {
                    if (ends_with_txt(item->name)) {
                        char target_path[256];
                        if (strcmp(current_dir_path, "0:") == 0) {
                            snprintf(target_path, sizeof(target_path), "0:/%s", item->name);
                        } else {
                            snprintf(target_path, sizeof(target_path), "%s/%s", current_dir_path, item->name);
                        }
                        
                        if (strcmp(last_read.novel, target_path) == 0 && last_read.page > 1) {
                            file_filter_mode = FILTER_NONE;
                            enter_resume_prompt();
                        } else {
                            strncpy(last_read.novel, target_path, 255);
                            last_read.novel[255] = '\0';
                            last_read.chapter = 1;
                            last_read.chapter_count = 1;
                            last_read.page = 1;
                            UI_SaveLastReadToStorage(&last_read);
                            file_filter_mode = FILTER_NONE;
                            enter_reading();
                        }
                    } else if ((file_filter_mode == FILTER_JSON || file_filter_mode == FILTER_JSON_CMD) && ends_with_json(item->name)) {
                        char target_path[256];
                        snprintf(target_path, sizeof(target_path), "%s/%s", current_dir_path, item->name);
                        FIL fp;
                        if (f_open(&fp, target_path, FA_READ) == FR_OK) {
                            char buf[512] = {0};
                            UINT br;
                            f_read(&fp, buf, sizeof(buf)-1, &br);
                            f_close(&fp);
                            
                            char *keys_start = strstr(buf, "\"keys\"");
                            if (!keys_start) keys_start = strstr(buf, "\"cmd\""); // Support both formats
                            
                            if (keys_start) {
                                keys_start = strchr(keys_start + 5, '\"');
                                if (keys_start) {
                                    keys_start++;
                                    char *keys_end = keys_start;
                                    while (*keys_end) {
                                        if (*keys_end == '\"' && *(keys_end - 1) != '\\') {
                                            break;
                                        }
                                        keys_end++;
                                    }
                                    if (*keys_end == '\"') {
                                        *keys_end = '\0';
                                        // Unescape \" to "
                                        char *src = keys_start;
                                        char *dst = keys_start;
                                        while (*src) {
                                            if (*src == '\\' && *(src + 1) == '\"') {
                                                src++; // Skip backslash
                                            }
                                            *dst++ = *src++;
                                        }
                                        *dst = '\0';
                                        if (file_filter_mode == FILTER_JSON_CMD) {
                                            extern void start_typing_cmd(const char *cmd);
                                            start_typing_cmd(keys_start);
                                            draw_text(16, 48, "正在同步电脑数据...", 0);
                                        } else {
                                            extern void start_typing(const char *text);
                                            start_typing(keys_start);
                                            draw_text(16, 48, "正在输出键盘指令...", 0);
                                        }
                                        
                                        render_header(item->name);
                                        display_full();
                                    }
                                }
                            }
                        }
                    } else {
                        render_header(current_dir_path);
                        draw_text(16, 48, "无法打开文件", 0);
                        display_full();
                    }
                }
            }
        } else if (event == UI_BUTTON_DOUBLE) {
            if (browser_item_count > 0) {
                browser_index = (browser_index + 1) % browser_item_count;
                render_file_browser();
                display_partial_landscape(0, 32, UI_W, UI_H - 32);
            }
        } else if (event == UI_BUTTON_LONG) {
            if (strcmp(current_dir_path, "0:") == 0 || file_filter_mode != FILTER_NONE) {
                file_filter_mode = FILTER_NONE;
                enter_settings();
            } else {
                char *last_slash = strrchr(current_dir_path, '/');
                if (last_slash) {
                    *last_slash = '\0';
                } else {
                    strcpy(current_dir_path, "0:");
                }
                enter_file_browser();
            }
        }
        break;

    case PAGE_RESUME_PROMPT:
        if (event == UI_BUTTON_SHORT) {
            enter_reading();
        } else if (event == UI_BUTTON_DOUBLE) {
            last_read.page = 1;
            UI_SaveLastReadToStorage(&last_read);
            enter_reading();
        } else if (event == UI_BUTTON_LONG) {
            enter_file_browser();
        }
        break;

    // ================================================================
    // TABLE OF CONTENTS PAGE
    //   Short  → confirm: enter reading at highlighted chapter
    //   Double → next:    move highlight to next chapter
    //   Long   → back:    return to main
    // ================================================================
    case PAGE_TOC:
        if (event == UI_BUTTON_SHORT) {
            enter_reading();
        } else if (event == UI_BUTTON_DOUBLE) {
            // No TOC for TXT
        } else if (event == UI_BUTTON_LONG) {
            enter_main();
        }
        break;

    // ================================================================
    // READING PAGE
    //   Short  → confirm: next page (or next chapter)
    //   Double → next:    next chapter (cycle back to 1)
    //   Long   → back:    return to main
    // ================================================================
    case PAGE_READING:
        if (event == UI_BUTTON_SHORT) {
            // Next Page
            if (sd_mounted && last_read.novel[0] != '\0') {
                uint32_t total = txt_has_idx ? txt_total_pages_idx : txt_total_pages;
                if (!txt_has_idx) {
                    scan_txt_to_page(last_read.page + 1);
                    total = txt_total_pages;
                }
                if (last_read.page < total) {
                    last_read.page++;
                    UI_SaveLastReadToStorage(&last_read);
                    render_reading();
                    display_partial_landscape(0, 32, UI_W, UI_H - 32);
                }
            }
        } else if (event == UI_BUTTON_DOUBLE) {
            // Prev Page
            if (sd_mounted && last_read.novel[0] != '\0') {
                if (last_read.page > 1) {
                    last_read.page--;
                    UI_SaveLastReadToStorage(&last_read);
                    render_reading();
                    display_partial_landscape(0, 32, UI_W, UI_H - 32);
                }
            }
        } else if (event == UI_BUTTON_LONG) {
            enter_main();
        }
        break;
    }
}
