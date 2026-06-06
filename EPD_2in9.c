#include "EPD_2in9.h"

#define EPD_W  128
#define EPD_H  296

// === LUT ===
static const uint8_t LUT_FULL[] = {
    0x50, 0xAA, 0x55, 0xAA, 0x11, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0x1F, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t LUT_PARTIAL[] = {
    0x10, 0x18, 0x18, 0x08, 0x18, 0x18,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x13, 0x14, 0x44, 0x12,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// === 8x16 Font ===
typedef struct {
    char ch;
    uint8_t row[16];
} EPD_Glyph8x16;

static const EPD_Glyph8x16 FONT8x16[95] = {
    {' ',{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
    {'!',{0,0,24,60,60,60,24,24,24,0,24,24,0,0,0,0}},
    {'"',{0,102,102,102,36,0,0,0,0,0,0,0,0,0,0,0}},
    {'#',{0,0,0,108,108,254,108,108,108,254,108,108,0,0,0,0}},
    {'$',{24,24,124,198,194,192,124,6,6,134,198,124,24,24,0,0}},
    {'%',{0,0,0,0,194,198,12,24,48,96,198,134,0,0,0,0}},
    {'&',{0,0,56,108,108,56,118,220,204,204,204,118,0,0,0,0}},
    {'\'',{0,48,48,48,96,0,0,0,0,0,0,0,0,0,0,0}},
    {'(',{0,0,12,24,48,48,48,48,48,48,24,12,0,0,0,0}},
    {')',{0,0,48,24,12,12,12,12,12,12,24,48,0,0,0,0}},
    {'*',{0,0,0,0,0,102,60,255,60,102,0,0,0,0,0,0}},
    {'+',{0,0,0,0,0,24,24,126,24,24,0,0,0,0,0,0}},
    {',',{0,0,0,0,0,0,0,0,0,24,24,24,48,0,0,0}},
    {'-',{0,0,0,0,0,0,0,254,0,0,0,0,0,0,0,0}},
    {'.',{0,0,0,0,0,0,0,0,0,0,24,24,0,0,0,0}},
    {'/',{0,0,0,0,2,6,12,24,48,96,192,128,0,0,0,0}},
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
    {':',{0,0,0,0,24,24,0,0,0,24,24,0,0,0,0,0}},
    {';',{0,0,0,0,24,24,0,0,0,24,24,48,0,0,0,0}},
    {'<',{0,0,0,6,12,24,48,96,48,24,12,6,0,0,0,0}},
    {'=',{0,0,0,0,0,126,0,0,126,0,0,0,0,0,0,0}},
    {'>',{0,0,0,96,48,24,12,6,12,24,48,96,0,0,0,0}},
    {'?',{0,0,124,198,198,12,24,24,24,0,24,24,0,0,0,0}},
    {'@',{0,0,0,124,198,198,222,222,222,220,192,124,0,0,0,0}},
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
    {'[',{0,0,60,48,48,48,48,48,48,48,48,60,0,0,0,0}},
    {'\\',{0,0,0,128,192,224,112,56,28,14,6,2,0,0,0,0}},
    {']',{0,0,60,12,12,12,12,12,12,12,12,60,0,0,0,0}},
    {'^',{16,56,108,198,0,0,0,0,0,0,0,0,0,0,0,0}},
    {'_',{0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0}},
    {'`',{48,48,24,0,0,0,0,0,0,0,0,0,0,0,0,0}},
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
    {'{',{0,0,14,24,24,24,112,24,24,24,24,14,0,0,0,0}},
    {'|',{0,0,24,24,24,24,24,24,24,24,24,24,0,0,0,0}},
    {'}',{0,0,112,24,24,24,14,24,24,24,24,112,0,0,0,0}},
    {'~',{0,0,118,220,0,0,0,0,0,0,0,0,0,0,0,0}},
};

// === Framebuffer ===
static uint8_t fb[4736];

// String utils
static int  sl(const char *s)       { int n=0; while(*s++)n++; return n; }

void my_memset(uint8_t *p, uint8_t v, int n) { for(int i=0;i<n;i++) p[i]=v; }

// === Low-level SPI & panel control ===
static void WaitIdle(void)  {
    int timeout = 2000; // 2 s safety – prevents permanent hang if BUSY is stuck
    while(DEV_Digital_Read(EPD_BUSY_PIN)==1 && --timeout > 0) DEV_Delay_ms(10);
}
static void SendCmd(uint8_t c) { DEV_Digital_Write(EPD_DC_PIN,0); DEV_Digital_Write(EPD_CS_PIN,0); DEV_SPI_WriteByte(c); DEV_Digital_Write(EPD_CS_PIN,1); }
static void SendData(uint8_t d){ DEV_Digital_Write(EPD_DC_PIN,1); DEV_Digital_Write(EPD_CS_PIN,0); DEV_SPI_WriteByte(d); DEV_Digital_Write(EPD_CS_PIN,1); }

static void Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN,1); DEV_Delay_ms(200);
    DEV_Digital_Write(EPD_RST_PIN,0); DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN,1); DEV_Delay_ms(200);
}

static void SetWindow(int xs,int ys,int xe,int ye)
{
    SendCmd(0x44); SendData((xs>>3)&0xFF); SendData((xe>>3)&0xFF);
    SendCmd(0x45); SendData(ys&0xFF); SendData((ys>>8)&0xFF); SendData(ye&0xFF); SendData((ye>>8)&0xFF);
}
static void SetCursor(int xs,int ys)
{
    SendCmd(0x4E); SendData((xs>>3)&0xFF);
    SendCmd(0x4F); SendData(ys&0xFF); SendData((ys>>8)&0xFF);
}
static void TurnOn(void) { SendCmd(0x22); SendData(0xC4); SendCmd(0x20); SendCmd(0xFF); WaitIdle(); }
static void TurnOnPartial(void) { SendCmd(0x22); SendData(0x0C); SendCmd(0x20); WaitIdle(); }

// === Drawing ===
static void DrawChar(int x,int y,char ch)
{
    if(ch<0x20||ch>0x7E) ch=0x20;
    const uint8_t *g = FONT8x16[ch-0x20].row;
    for(int row=0;row<16;row++){
        int py=y+row; if(py<0||py>=EPD_H) continue;
        uint8_t bits=g[row];
        for(int col=0;col<8;col++){
            int px=x+col; if(px<0||px>=EPD_W) continue;
            int bi=(px>>3)+py*(EPD_W/8);
            int bit=7-(px&0x07);
            if(bits&(0x80>>col)) fb[bi]&=~(1<<bit);
            else                 fb[bi]|=(1<<bit);
        }
    }
}

static void DrawString(int x,int y,const char *s)
{
    while(*s){ DrawChar(x,y,*s); x+=8; s++; }
}

static void Flush(void)
{
    int w=EPD_W/8;
    SetWindow(0,0,EPD_W,EPD_H);
    for(int yy=0;yy<EPD_H;yy++){ SetCursor(0,yy); SendCmd(0x24); for(int xx=0;xx<w;xx++) SendData(fb[yy*w+xx]); }
    TurnOn();
}

static void FbClear(void) { my_memset(fb,0xFF,sizeof(fb)); }

// ================================================================
// Public API
// ================================================================

uint8_t EPD_2IN9_Init(uint8_t Mode)
{
    Reset();
    SendCmd(0x01); SendData((EPD_H-1)&0xFF); SendData(((EPD_H-1)>>8)&0xFF); SendData(0x00);
    SendCmd(0x0C); SendData(0xD7); SendData(0xD6); SendData(0x9D);
    SendCmd(0x2C); SendData(0xA8);
    SendCmd(0x3A); SendData(0x1A);
    SendCmd(0x3B); SendData(0x08);
    SendCmd(0x3C); SendData((Mode == EPD_2IN9_PART) ? 0x80 : 0x03);
    SendCmd(0x11); SendData(0x03);
    
    SendCmd(0x32); 
    if (Mode == EPD_2IN9_PART) {
        for(int i=0; i<30; i++) SendData(LUT_PARTIAL[i]);
    } else {
        for(int i=0; i<30; i++) SendData(LUT_FULL[i]);
    }
    
    FbClear();
    return 0;
}

uint8_t EPD_2IN9_Clear(void) { FbClear(); Flush(); return 0; }

void EPD_2IN9_Display(const char *text)
{
    FbClear();
    int l=sl(text),x=(EPD_W-l*8)/2,y=(EPD_H-16)/2;
    if(x<0)x=0; DrawString(x,y,text); Flush();
}

// Display 3 centered text lines
void EPD_2IN9_DisplayLines(const char *l1,const char *l2,const char *l3)
{
    FbClear();
    if(l1&&*l1){ int l=sl(l1); int x=(EPD_W-l*8)/2; if(x<0)x=0; DrawString(x,50,l1); }
    if(l2&&*l2){ int l=sl(l2); int x=(EPD_W-l*8)/2; if(x<0)x=0; DrawString(x,140,l2); }
    if(l3&&*l3){ int l=sl(l3); int x=(EPD_W-l*8)/2; if(x<0)x=0; DrawString(x,230,l3); }
    Flush();
}

// Load background bitmap, then overlay text
void EPD_2IN9_DisplayWithBG(const uint8_t *bg_bitmap, const char *l1, const char *l2, const char *l3)
{
    // Copy background bitmap into framebuffer
    my_memset(fb, 0xFF, sizeof(fb));  // Start white
    if(bg_bitmap) {
        for(int i=0;i<(int)sizeof(fb);i++) fb[i]=bg_bitmap[i];
    }
    // Overlay text
    if(l1&&*l1){ int l=sl(l1); int x=(EPD_W-l*8)/2; if(x<0)x=0; DrawString(x,50,l1); }
    if(l2&&*l2){ int l=sl(l2); int x=(EPD_W-l*8)/2; if(x<0)x=0; DrawString(x,140,l2); }
    if(l3&&*l3){ int l=sl(l3); int x=(EPD_W-l*8)/2; if(x<0)x=0; DrawString(x,230,l3); }
    Flush();
}

void EPD_2IN9_DisplayWindow(const uint8_t *full_frame, int x, int y, int w, int h)
{
    int x0, x1, y0, y1;
    int bx0, bx1;

    if(!full_frame || w <= 0 || h <= 0) return;

    x0 = x;
    y0 = y;
    x1 = x + w - 1;
    y1 = y + h - 1;

    if(x0 < 0) x0 = 0;
    if(y0 < 0) y0 = 0;
    if(x1 >= EPD_W) x1 = EPD_W - 1;
    if(y1 >= EPD_H) y1 = EPD_H - 1;
    if(x0 > x1 || y0 > y1) return;

    // The controller addresses X in bytes, so expand to byte boundaries.
    x0 &= ~0x07;
    x1 |= 0x07;
    if(x1 >= EPD_W) x1 = EPD_W - 1;

    bx0 = x0 >> 3;
    bx1 = x1 >> 3;

    SendCmd(0x91);  // partial in
    SetWindow(x0, y0, x1, y1);

    for(int yy = y0; yy <= y1; yy++) {
        SetCursor(x0, yy);
        SendCmd(0x24);
        for(int bx = bx0; bx <= bx1; bx++) {
            SendData(full_frame[yy * (EPD_W / 8) + bx]);
        }
    }

    TurnOnPartial();
    SendCmd(0x92);  // partial out
}

void EPD_2IN9_Sleep(void) { SendCmd(0x10); SendData(0x01); DEV_Delay_ms(100); }

// ================================================================
// Drawing primitives (operate on framebuffer; call EPD_Flush after)
// ================================================================

void EPD_DrawPixel(int x, int y, int black)
{
    if(x < 0 || x >= EPD_W || y < 0 || y >= EPD_H) return;
    int bi  = (x >> 3) + y * (EPD_W / 8);
    int bit = 7 - (x & 0x07);
    if(black) fb[bi] &= (uint8_t)~(1 << bit);
    else      fb[bi] |= (uint8_t) (1 << bit);
}

void EPD_DrawHLine(int x, int y, int w, int black)
{
    for(int i = 0; i < w; i++) EPD_DrawPixel(x + i, y, black);
}

void EPD_DrawVLine(int x, int y, int h, int black)
{
    for(int i = 0; i < h; i++) EPD_DrawPixel(x, y + i, black);
}

void EPD_DrawBox(int x, int y, int w, int h, int black)
{
    EPD_DrawHLine(x, y, w, black);
    EPD_DrawHLine(x, y + h - 1, w, black);
    EPD_DrawVLine(x, y, h, black);
    EPD_DrawVLine(x + w - 1, y, h, black);
}

void EPD_DrawText(int x, int y, const char *s)
{
    DrawString(x, y, s);
}

void EPD_FbClear(void) { FbClear(); }

void EPD_Flush(void) { Flush(); }
