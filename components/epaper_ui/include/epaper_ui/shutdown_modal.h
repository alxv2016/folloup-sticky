#ifndef EPAPER_UI_SHUTDOWN_MODAL_H_
#define EPAPER_UI_SHUTDOWN_MODAL_H_

#include <cstdint>

#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

enum class ShutdownModalAction : uint8_t {
    kCancel = 0,
    kConfirm = 1,
};

struct ShutdownModalState {
    bool visible = false;
    ShutdownModalAction selected_action = ShutdownModalAction::kCancel;
};

UiRect ShutdownModalCardBounds(int portrait_width, int portrait_height);
UiRect ShutdownModalActionBounds(int portrait_width,
                                 int portrait_height,
                                 ShutdownModalAction action);
ShutdownModalAction HitTestShutdownModalAction(int portrait_width,
                                               int portrait_height,
                                               int x,
                                               int y,
                                               bool* hit);
void DrawShutdownModal(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const ShutdownModalState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SHUTDOWN_MODAL_H_
