#include "epaper_ui/font_renderer.h"

#include <algorithm>
#include <cstdint>

#include "epaper_ui/bitmap_font.h"
#include "epaper_ui/generated_epaper_fonts.h"

namespace epaper_ui {
namespace {

struct FontSelection {
    const epaper_font::BitmapFont* font = nullptr;
    int tracking = 0;
    int scale_numerator = 1;
    int scale_denominator = 1;
    int line_height_override = 0;
};

int ScaleMetric(int value, const FontSelection& selection)
{
    const int64_t scaled = static_cast<int64_t>(value) * selection.scale_numerator;
    if (scaled >= 0) {
        return static_cast<int>((scaled + (selection.scale_denominator / 2)) /
                                selection.scale_denominator);
    }
    return -static_cast<int>(((-scaled) + (selection.scale_denominator / 2)) /
                             selection.scale_denominator);
}

FontSelection MakeFontSelection(const design::TypographySpec& spec,
                                int base_size,
                                const epaper_font::BitmapFont& semibold,
                                const epaper_font::BitmapFont& bold,
                                const epaper_font::BitmapFont& black)
{
    const epaper_font::BitmapFont* font = &semibold;
    switch (spec.weight) {
        case design::font_weight::kBlack:
            font = &black;
            break;
        case design::font_weight::kBold:
            font = &bold;
            break;
        case design::font_weight::kSemibold:
        default:
            font = &semibold;
            break;
    }

    return {
        .font = font,
        .tracking = 0,
        .scale_numerator = std::max(1, spec.size) * std::max(1, spec.scale),
        .scale_denominator = std::max(1, base_size),
        .line_height_override = spec.line_height_override,
    };
}

FontSelection FontForRole(design::TypographyRole role)
{
    const design::TypographySpec spec = design::TypographySpecForRole(role);
    if (spec.size == design::typography::k22) {
        return MakeFontSelection(spec, 22, epaper_fonts::kInter22SemiBold,
                                 epaper_fonts::kInter22Bold, epaper_fonts::kInter22Black);
    }
    if (spec.size == design::typography::k26) {
        return MakeFontSelection(spec, 26, epaper_fonts::kInter26SemiBold,
                                 epaper_fonts::kInter26Bold, epaper_fonts::kInter26Black);
    }
    if (spec.size == design::typography::k32) {
        return MakeFontSelection(spec, 32, epaper_fonts::kInter32SemiBold,
                                 epaper_fonts::kInter32Bold, epaper_fonts::kInter32Black);
    }
    if (spec.size == design::typography::k38) {
        return MakeFontSelection(spec, 38, epaper_fonts::kInter38SemiBold,
                                 epaper_fonts::kInter38Bold, epaper_fonts::kInter38Black);
    }
    if (spec.size == design::typography::k46) {
        return MakeFontSelection(spec, 46, epaper_fonts::kInter46SemiBold,
                                 epaper_fonts::kInter46Bold, epaper_fonts::kInter46Black);
    }
    if (spec.size == design::typography::k55) {
        return MakeFontSelection(spec, 55, epaper_fonts::kInter55SemiBold,
                                 epaper_fonts::kInter55Bold, epaper_fonts::kInter55Black);
    }

    return MakeFontSelection(spec, 165, epaper_fonts::kInter165SemiBold,
                             epaper_fonts::kInter165Bold, epaper_fonts::kInter165Black);
}

}  // namespace

int MeasureText(design::TypographyRole role, std::string_view text)
{
    const FontSelection selection = FontForRole(role);
    return ScaleMetric(epaper_font::MeasureText(*selection.font, text, selection.tracking),
                       selection);
}

int LineHeight(design::TypographyRole role)
{
    const FontSelection selection = FontForRole(role);
    if (selection.line_height_override > 0) {
        return selection.line_height_override;
    }
    return ScaleMetric(selection.font->line_height, selection);
}

void DrawText(const DrawPixelFn& draw_pixel,
              int x,
              int y,
              std::string_view text,
              uint8_t color,
              design::TypographyRole role)
{
    const FontSelection selection = FontForRole(role);
    int cursor_x = x;
    for (size_t index = 0; index < text.size(); ++index) {
        const epaper_font::GlyphBitmap* glyph =
            epaper_font::FindGlyph(*selection.font, text[index]);
        if (glyph == nullptr) {
            continue;
        }

        const int glyph_x = cursor_x + ScaleMetric(glyph->bearing_x, selection);
        const int glyph_y = y + ScaleMetric(selection.font->ascent, selection) -
                            ScaleMetric(glyph->bearing_y, selection);

        for (int row = 0; row < glyph->height; ++row) {
            const int row_start = glyph_y + ScaleMetric(row, selection);
            const int row_end = glyph_y + ScaleMetric(row + 1, selection);
            if (row_end <= row_start) {
                continue;
            }

            for (int col = 0; col < glyph->width; ++col) {
                const size_t bit_index = static_cast<size_t>(row) * glyph->width + col;
                const size_t byte_index = glyph->bitmap_offset + (bit_index / 8U);
                if (byte_index >= glyph->bitmap_offset + glyph->bitmap_byte_count) {
                    continue;
                }

                const uint8_t bit_mask = static_cast<uint8_t>(0x80U >> (bit_index % 8U));
                if ((selection.font->bitmaps[byte_index] & bit_mask) == 0) {
                    continue;
                }

                const int col_start = glyph_x + ScaleMetric(col, selection);
                const int col_end = glyph_x + ScaleMetric(col + 1, selection);
                if (col_end <= col_start) {
                    continue;
                }

                for (int scaled_y = row_start; scaled_y < row_end; ++scaled_y) {
                    for (int scaled_x = col_start; scaled_x < col_end; ++scaled_x) {
                        draw_pixel(scaled_x, scaled_y, color);
                    }
                }
            }
        }

        cursor_x += ScaleMetric(glyph->advance, selection);
        if (index + 1 < text.size()) {
            cursor_x += ScaleMetric(selection.tracking, selection);
        }
    }
}

}  // namespace epaper_ui
