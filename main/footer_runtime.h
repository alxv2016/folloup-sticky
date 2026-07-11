#ifndef FOOTER_RUNTIME_H_
#define FOOTER_RUNTIME_H_

#include <cstdint>
#include <mutex>

#include "app_interaction_result.h"
#include "app_interaction_target.h"
#include "display_service.h"
#include "epaper_ui/global_footer.h"
#include "esp_err.h"

namespace footer_runtime {

enum class FooterFocusItem : uint8_t {
    kNone = 0,
    kHome,
    kSettings,
    kWifi,
    kTime,
    kFolder,
    kMic,
    kSticky,
};

struct LayoutState {
    bool visible = true;
    bool show_home = false;
    bool show_settings = false;
    bool show_wifi = false;
    bool show_time = false;
    bool show_folder = false;
    bool show_mic = true;
    bool show_sticky = false;
};

// Future page ports should project shared page focus into this state instead of
// letting the footer own a standalone navigation index.
struct ProjectionState {
    FooterFocusItem focused_item = FooterFocusItem::kNone;
};

using ActivateHandler = app_interaction::InputResult (*)(FooterFocusItem item, void* context);

void SetActivateHandler(ActivateHandler handler, void* context);
void SetLayoutState(const LayoutState& state);
void SetProjectionState(const ProjectionState& state);
LayoutState GetLayoutState();
ProjectionState GetProjectionState();
bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
bool FocusTouchTarget(const app_interaction::InteractiveTarget& target);
app_interaction::InputResult ActivateTouchTarget(const app_interaction::InteractiveTarget& target);
epaper_ui::GlobalFooterState BuildState();
esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRefreshNow(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);

}  // namespace footer_runtime

#endif  // FOOTER_RUNTIME_H_
