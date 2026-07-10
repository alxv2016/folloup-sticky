#ifndef EPAPER_UI_SETTINGS_PAGE_H_
#define EPAPER_UI_SETTINGS_PAGE_H_

#include <cstdint>
#include <string_view>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/menu_toggle.h"
#include "epaper_ui/sd_status.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

enum class SettingsPageItemId : uint8_t {
    kNone = 0,
    kWifiToggle,
    kAccessPointToggle,
    kFormatSdButton,
    kManualOnboardingButton,
};

struct SettingsPageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "Settings";
    MenuToggleState wifi_toggle = {};
    MenuToggleState access_point_toggle = {};
    SdStatusState storage_status = {};
    ButtonState format_sd_button = {};
    ButtonState manual_onboarding_button = {};
};

UiRect SettingsPageItemBounds(int portrait_width,
                              int portrait_height,
                              const SettingsPageState& state,
                              SettingsPageItemId item);
UiRect SettingsPageItemVisualBounds(int portrait_width,
                                    int portrait_height,
                                    const SettingsPageState& state,
                                    SettingsPageItemId item);
bool HitTestSettingsPageItem(int portrait_width,
                             int portrait_height,
                             const SettingsPageState& state,
                             int x,
                             int y,
                             SettingsPageItemId* item);
void DrawSettingsPage(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const SettingsPageState& state,
                      const StatusBarState& status_bar_state,
                      const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SETTINGS_PAGE_H_
