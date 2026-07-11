#ifndef OVERLAY_RUNTIME_H_
#define OVERLAY_RUNTIME_H_

#include <string>
#include <vector>

#include "app_interaction_result.h"
#include "app_interaction_target.h"
#include "button_service.h"
#include "epaper_ui/keyboard.h"
#include "epaper_ui/keyboard_controller.h"
#include "epaper_ui/select_modal.h"
#include "epaper_ui/card_modal.h"
#include "epaper_ui/sticky_note.h"
#include "epaper_ui/toast.h"
#include "esp_err.h"

namespace overlay_runtime {

// One sticky in the full-page sticky-note overlay. Content mirrors the vibe card / timeline row.
struct StickyNoteItem {
    std::string tag_text;
    epaper_ui::ListItemHeaderState header;
    std::string body_text;
};

using KeyboardEventHandler =
    void (*)(const epaper_ui::KeyboardState& state, epaper_ui::KeyboardIntent intent, void* context);

esp_err_t Init();
bool IsShutdownModalVisible();
bool IsStorageModalVisible();
bool IsSelectModalVisible();
bool IsKeyboardVisible();
bool IsStickyNoteVisible();
bool IsShutdownPending();
bool IsInputCaptured();

esp_err_t ShowShutdownModal();
esp_err_t DismissShutdownModal();
esp_err_t ShowStorageModalNoSdCard();
esp_err_t ShowStorageModalConfirmFormat();
esp_err_t ShowStorageModalFormatting();
esp_err_t ShowStorageModalFormatSuccess();
esp_err_t ShowStorageModalFormatError();
esp_err_t DismissStorageModal();
esp_err_t ShowSelectModal(const epaper_ui::SelectModalState& state);
esp_err_t DismissSelectModal();
esp_err_t ShowKeyboard(const epaper_ui::KeyboardState& state,
                       KeyboardEventHandler handler,
                       void* context);
esp_err_t UpdateKeyboardState(const epaper_ui::KeyboardState& state);
esp_err_t DismissKeyboard();
bool MoveFocus(int delta);
bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
bool FocusTouchTarget(const app_interaction::InteractiveTarget& target);
app_interaction::InputResult ActivateTouchTarget(const app_interaction::InteractiveTarget& target);
void SetShutdownRequestInProgress(bool in_progress);
bool TakePendingFeedback(app_interaction::FeedbackCue* cue);

app_interaction::InputResult HandleButtonEvent(const button_service::ButtonEventInfo& event);

esp_err_t ShowToast(const epaper_ui::ToastState& state);
esp_err_t ShowToastForDuration(const epaper_ui::ToastState& state, uint32_t duration_ms);
esp_err_t ClearToast();

// Full-page sticky-note overlay: shows `items` starting at the first, cycling with wrap-around via
// prev/next and dismissing via Close. A no-op (returns ESP_ERR_INVALID_ARG) when `items` is empty.
esp_err_t ShowStickyNotes(const std::vector<StickyNoteItem>& items);
esp_err_t DismissStickyNotes();

}  // namespace overlay_runtime

#endif  // OVERLAY_RUNTIME_H_
