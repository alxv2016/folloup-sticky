#ifndef EPAPER_UI_GLOBAL_FOOTER_H_
#define EPAPER_UI_GLOBAL_FOOTER_H_

#include <cstdint>

#include "epaper_ui/overlay_geometry.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

enum class GlobalFooterItemId : uint8_t {
    kNone = 0,
    kHome,
    kSettings,
    kWifi,
    kTime,
    kFolder,
    kMic,
};

struct FooterButtonState {
    bool visible = false;
    bool selected = false;
    const EmbeddedImageAsset* icon = nullptr;
};

struct FooterMicState {
    bool visible = true;
    bool active = false;
    bool selected = false;
    const EmbeddedImageAsset* idle_icon = nullptr;
    const EmbeddedImageAsset* active_icon = nullptr;
};

struct GlobalFooterState {
    bool visible = false;
    FooterButtonState home = {};
    FooterButtonState settings = {};
    FooterButtonState wifi = {};
    FooterButtonState time = {};
    FooterButtonState folder = {};
    FooterMicState mic = {};
};

UiRect GlobalFooterBounds(int portrait_width, int portrait_height, const GlobalFooterState& state);
UiRect GlobalFooterItemBounds(int portrait_width,
                              int portrait_height,
                              const GlobalFooterState& state,
                              GlobalFooterItemId item);
bool HitTestGlobalFooterItem(int portrait_width,
                             int portrait_height,
                             const GlobalFooterState& state,
                             int x,
                             int y,
                             GlobalFooterItemId* item);
void DrawGlobalFooter(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const GlobalFooterState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_GLOBAL_FOOTER_H_
