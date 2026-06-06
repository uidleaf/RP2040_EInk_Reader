import os
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "C:\\Windows\\Fonts\\msyh.ttc"
FONT_SIZE = 12 # YaHei looks better when slightly smaller to fit 16x16 without clipping, or 14. We'll use 14.

def generate_font():
    try:
        font = ImageFont.truetype(FONT_PATH, 14)
    except IOError:
        print("Cannot load font.")
        return

    glyphs = []
    
    # 1. ASCII 0x20 - 0x7E
    for code in range(0x20, 0x7E + 1):
        char = chr(code)
        glyphs.append((code, char))
        
    # 2. Add some common symbols from GB2312 (Qu 1-9), typically punctuation.
    # Actually, let's just generate all valid Level 1 and Level 2 Chinese characters.
    for hi in range(0xB0, 0xF8):
        for lo in range(0xA1, 0xFF):
            try:
                char = bytes([hi, lo]).decode('gb2312')
                code = ord(char)
                glyphs.append((code, char))
            except UnicodeDecodeError:
                pass
                
    # Also add specific characters in case they are missing
    special_chars = "无容量目录设置退出卡"
    for char in special_chars:
        code = ord(char)
        if not any(g[0] == code for g in glyphs):
            glyphs.append((code, char))

    # Sort by unicode code point for binary search
    glyphs.sort(key=lambda x: x[0])
    
    # Remove duplicates
    unique_glyphs = []
    seen = set()
    for code, char in glyphs:
        if code not in seen:
            seen.add(code)
            unique_glyphs.append((code, char))
            
    print(f"Generating {len(unique_glyphs)} characters...")

    with open("font_gb2312.c", "w", encoding="utf-8") as fc, open("font_gb2312.h", "w", encoding="utf-8") as fh:
        fh.write("#ifndef __FONT_GB2312_H\n")
        fh.write("#define __FONT_GB2312_H\n\n")
        fh.write("#include <stdint.h>\n\n")
        fh.write("typedef struct {\n")
        fh.write("    uint16_t unicode;\n")
        fh.write("    uint8_t bitmap[32];\n")
        fh.write("} Glyph16x16;\n\n")
        fh.write("extern const Glyph16x16 FONT_GB2312[];\n")
        fh.write(f"extern const int FONT_GB2312_SIZE;\n\n")
        fh.write("const uint8_t* font_get_glyph(uint16_t unicode);\n\n")
        fh.write("#endif\n")

        fc.write("#include \"font_gb2312.h\"\n")
        fc.write("#include <stddef.h>\n\n")
        fc.write("const Glyph16x16 FONT_GB2312[] = {\n")
        
        for code, char in unique_glyphs:
            img = Image.new('1', (16, 16), color=0)
            draw = ImageDraw.Draw(img)
            
            # Center the character
            # For Pillow > 8.0.0, use textbbox. For older, use textsize.
            try:
                left, top, right, bottom = draw.textbbox((0, 0), char, font=font)
                w = right - left
                h = bottom - top
            except AttributeError:
                w, h = draw.textsize(char, font=font)
                
            x = (16 - w) // 2
            y = (16 - h) // 2 - 2 # minor manual offset
            
            draw.text((x, y), char, font=font, fill=1)
            
            # Convert to 32-byte array
            bitmap = []
            for row in range(16):
                byte1 = 0
                byte2 = 0
                for col in range(8):
                    if img.getpixel((col, row)):
                        byte1 |= (1 << (7 - col))
                for col in range(8, 16):
                    if img.getpixel((col, row)):
                        byte2 |= (1 << (15 - col))
                bitmap.append(byte1)
                bitmap.append(byte2)
            
            bitmap_str = ", ".join(f"0x{b:02X}" for b in bitmap)
            fc.write(f"    {{ 0x{code:04X}, {{ {bitmap_str} }} }}, // {char}\n")
            
        fc.write("};\n\n")
        fc.write(f"const int FONT_GB2312_SIZE = {len(unique_glyphs)};\n\n")
        fc.write(
'''const uint8_t* font_get_glyph(uint16_t unicode) {
    int left = 0;
    int right = FONT_GB2312_SIZE - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (FONT_GB2312[mid].unicode == unicode) {
            return FONT_GB2312[mid].bitmap;
        }
        if (FONT_GB2312[mid].unicode < unicode) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return NULL;
}
''')

if __name__ == "__main__":
    generate_font()
    print("Done!")
