#ifndef BITMAP_FONT_H
#define BITMAP_FONT_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace epaper_font {

struct GlyphBitmap {
    uint32_t bitmap_offset;
    uint16_t bitmap_byte_count;
    uint8_t width;
    uint8_t height;
    int16_t bearing_x;
    int16_t bearing_y;
    uint8_t advance;
};

struct BitmapFont {
    const char* name;
    uint8_t first_char;
    uint8_t last_char;
    uint8_t line_height;
    uint8_t ascent;
    const GlyphBitmap* glyphs;
    const uint8_t* bitmaps;
};

const GlyphBitmap* FindGlyph(const BitmapFont& font, char ch);
int MeasureText(const BitmapFont& font, std::string_view text, int tracking = 0);

}  // namespace epaper_font

#endif  // BITMAP_FONT_H
