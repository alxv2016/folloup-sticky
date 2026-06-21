#include "epaper_ui/bitmap_font.h"

#include <algorithm>

namespace epaper_font {

namespace {

char NormalizeGlyphChar(char ch) {
    unsigned char value = static_cast<unsigned char>(ch);
    if (value < 32 || value > 126) {
        return '?';
    }
    return static_cast<char>(value);
}

}  // namespace

const GlyphBitmap* FindGlyph(const BitmapFont& font, char ch) {
    const char normalized = NormalizeGlyphChar(ch);
    if (normalized < static_cast<char>(font.first_char) ||
        normalized > static_cast<char>(font.last_char)) {
        return nullptr;
    }

    const size_t index = static_cast<size_t>(
        static_cast<unsigned char>(normalized) - font.first_char);
    return &font.glyphs[index];
}

int MeasureText(const BitmapFont& font, std::string_view text, int tracking) {
    int width = 0;
    bool first = true;

    for (char ch : text) {
        if (ch == '\r' || ch == '\n') {
            break;
        }
        const GlyphBitmap* glyph = FindGlyph(font, ch);
        if (glyph == nullptr) {
            continue;
        }
        if (!first) {
            width += std::max(tracking, 0);
        }
        width += glyph->advance;
        first = false;
    }

    return width;
}

}  // namespace epaper_font
