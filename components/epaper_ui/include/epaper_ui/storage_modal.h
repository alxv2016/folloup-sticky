#ifndef EPAPER_UI_STORAGE_MODAL_H_
#define EPAPER_UI_STORAGE_MODAL_H_

#include <cstdint>

#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

enum class StorageModalKind : uint8_t {
    kNone = 0,
    kNoSdCard,
    kConfirmFormat,
    kFormatting,
    kFormatSuccess,
    kFormatError,
};

struct StorageModalState {
    bool visible = false;
    StorageModalKind kind = StorageModalKind::kNone;
    int selected_action_index = 0;
};

UiRect StorageModalPanelBounds(int portrait_width,
                               int portrait_height,
                               const StorageModalState& state);
UiRect StorageModalActionBounds(int portrait_width,
                                int portrait_height,
                                const StorageModalState& state,
                                int action_index);
int HitTestStorageModalAction(int portrait_width,
                              int portrait_height,
                              const StorageModalState& state,
                              int x,
                              int y,
                              bool* hit);
void DrawStorageModal(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const StorageModalState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_STORAGE_MODAL_H_
