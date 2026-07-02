#ifndef EPAPER_UI_RENDER_UTILS_H_
#define EPAPER_UI_RENDER_UTILS_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "design_tokens.h"
#include "asset_types.h"
#include "epaper_ui/font_renderer.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

int ClampPositive(int value);
// Greedy word-wrap of `text` into lines that each fit `max_width` for the given typography
// role. Returns the whole text as a single line when max_width <= 0 or it already fits.
std::vector<std::string> WrapTextToWidth(design::TypographyRole role,
                                         const std::string& text,
                                         int max_width);
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
// Nearest-neighbour scale a mono asset into `dest` (used to draw small source icons at a larger
// display size, e.g. 36px assets shown at the 64px empty-state size).
void DrawScaledPortraitMonoAsset(uint8_t* framebuffer,
                                 int raw_width,
                                 int raw_height,
                                 int portrait_width,
                                 int portrait_height,
                                 const UiRect& dest,
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
