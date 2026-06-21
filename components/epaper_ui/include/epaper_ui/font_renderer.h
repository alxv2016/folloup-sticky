#ifndef EPAPER_UI_FONT_RENDERER_H_
#define EPAPER_UI_FONT_RENDERER_H_

#include <cstdint>
#include <functional>
#include <string_view>

#include "design_tokens.h"

namespace epaper_ui {

using DrawPixelFn = std::function<void(int x, int y, uint8_t color)>;

int MeasureText(design::TypographyRole role, std::string_view text);
int LineHeight(design::TypographyRole role);
void DrawText(const DrawPixelFn& draw_pixel,
              int x,
              int y,
              std::string_view text,
              uint8_t color,
              design::TypographyRole role);

}  // namespace epaper_ui

#endif  // EPAPER_UI_FONT_RENDERER_H_
