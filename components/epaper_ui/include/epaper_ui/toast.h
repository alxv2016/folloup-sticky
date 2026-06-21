#ifndef EPAPER_UI_TOAST_H_
#define EPAPER_UI_TOAST_H_

#include <cstdint>
#include <string>

#include "epaper_ui/overlay_geometry.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

struct ToastState {
    bool visible = false;
    std::string body_text = {};
    const EmbeddedImageAsset* leading_icon = nullptr;
    bool show_close_button = false;
    bool close_button_focused = false;
};

UiRect ToastPanelBounds(int portrait_width, int portrait_height, const ToastState& state);
UiRect ToastCloseButtonBounds(int portrait_width, int portrait_height, const ToastState& state);
bool HitTestToastCloseButton(int portrait_width,
                             int portrait_height,
                             const ToastState& state,
                             int x,
                             int y);
void DrawToast(uint8_t* framebuffer,
               int raw_width,
               int raw_height,
               int portrait_width,
               int portrait_height,
               const ToastState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TOAST_H_
