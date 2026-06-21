#ifndef EPAPER_UI_RENDER_UTILS_H_
#define EPAPER_UI_RENDER_UTILS_H_

#include <cstdint>
#include <string_view>

#include "design_tokens.h"
#include "asset_types.h"
#include "epaper_ui/font_renderer.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

int ClampPositive(int value);
int CenterOffset(int container_size, int item_size);
bool ShouldDrawBlackForTone(int x, int y, uint8_t tone);
void DrawPortraitPixel(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       int x,
                       int y,
                       bool black);
void FillPortraitRect(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const UiRect& rect,
                      uint8_t tone);
void DrawPortraitBorder(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        const UiRect& rect,
                        int thickness,
                        uint8_t tone);
void FillRoundedPortraitRect(uint8_t* framebuffer,
                             int raw_width,
                             int raw_height,
                             int portrait_width,
                             int portrait_height,
                             const UiRect& rect,
                             int radius,
                             uint8_t tone);
void DrawRoundedPortraitBorder(uint8_t* framebuffer,
                               int raw_width,
                               int raw_height,
                               int portrait_width,
                               int portrait_height,
                               const UiRect& rect,
                               int radius,
                               int thickness,
                               uint8_t tone);
void DrawPortraitMonoAsset(uint8_t* framebuffer,
                           int raw_width,
                           int raw_height,
                           int portrait_width,
                           int portrait_height,
                           int x,
                           int y,
                           const EmbeddedImageAsset* asset,
                           uint8_t tone);
void DrawTypographyText(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        int x,
                        int y,
                        std::string_view text,
                        design::TypographyRole role,
                        uint8_t tone);

}  // namespace epaper_ui

#endif  // EPAPER_UI_RENDER_UTILS_H_
