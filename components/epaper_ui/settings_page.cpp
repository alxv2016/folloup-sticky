#include "epaper_ui/settings_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr auto kSectionRole = design::TypographyRole::kHeadingH2;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k16;
constexpr int kTitleBottomGap = design::spacing::k24;
constexpr int kNetworkHeadingGap = 0;
constexpr int kSectionGap = design::spacing::k24;
constexpr int kStorageStatusGap = design::spacing::k12;
constexpr int kStorageButtonTopGap = design::spacing::k16;

struct Layout {
    UiRect wifi_toggle = {};
    UiRect access_point_toggle = {};
    UiRect storage_status = {};
    UiRect format_sd_button = {};
};

Layout BuildLayout(int portrait_width, int portrait_height, const SettingsPageState& state)
{
    (void)portrait_height;
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_height = LineHeight(kTitleRole);

    const int network_heading_y = title_y + title_height + kTitleBottomGap;
    const int network_items_y =
        network_heading_y + LineHeight(kSectionRole) + kNetworkHeadingGap;

    MenuToggleStyle network_toggle_style = {};
    network_toggle_style.width = page_width;
    const UiRect wifi_toggle = MenuToggleBounds(page_x, network_items_y, network_toggle_style);

    MenuToggleStyle access_point_style = network_toggle_style;
    access_point_style.bottom_border_thickness = 0;
    const UiRect access_point_toggle =
        MenuToggleBounds(page_x, wifi_toggle.bottom(), access_point_style);

    const int storage_heading_y = access_point_toggle.bottom() + kSectionGap;
    SdStatusStyle storage_style = {};
    storage_style.max_width = page_width;
    const UiRect storage_status = SdStatusBounds(page_x,
                                                 storage_heading_y + LineHeight(kSectionRole) +
                                                     kStorageStatusGap,
                                                 state.storage_status,
                                                 storage_style);

    ButtonStyle format_button_style = {};
    format_button_style.width = page_width;
    const int button_y = storage_status.bottom() + kStorageButtonTopGap;
    const UiRect format_sd_button =
        ButtonBounds(page_x, button_y, state.format_sd_button, format_button_style);

    return {
        .wifi_toggle = wifi_toggle,
        .access_point_toggle = access_point_toggle,
        .storage_status = storage_status,
        .format_sd_button = format_sd_button,
    };
}

}  // namespace

UiRect SettingsPageItemBounds(int portrait_width,
                              int portrait_height,
                              const SettingsPageState& state,
                              SettingsPageItemId item)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    switch (item) {
        case SettingsPageItemId::kWifiToggle:
            return layout.wifi_toggle;
        case SettingsPageItemId::kAccessPointToggle:
            return layout.access_point_toggle;
        case SettingsPageItemId::kFormatSdButton:
            return layout.format_sd_button;
        case SettingsPageItemId::kNone:
        default:
            return {};
    }
}

bool HitTestSettingsPageItem(int portrait_width,
                             int portrait_height,
                             const SettingsPageState& state,
                             int x,
                             int y,
                             SettingsPageItemId* item)
{
    if (item != nullptr) {
        *item = SettingsPageItemId::kNone;
    }

    constexpr SettingsPageItemId kItems[] = {
        SettingsPageItemId::kWifiToggle,
        SettingsPageItemId::kAccessPointToggle,
        SettingsPageItemId::kFormatSdButton,
    };
    for (SettingsPageItemId candidate : kItems) {
        const UiRect bounds =
            SettingsPageItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (item != nullptr) {
                *item = candidate;
            }
            return true;
        }
    }

    return false;
}

void DrawSettingsPage(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const SettingsPageState& state,
                      const StatusBarState& status_bar_state,
                      const GlobalFooterState& footer_state)
{
    if (framebuffer == nullptr) {
        return;
    }

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {0, 0, portrait_width, portrait_height},
                     design::color::kWhite);
    DrawStatusBar(framebuffer,
                  raw_width,
                  raw_height,
                  portrait_width,
                  portrait_height,
                  status_bar_state);

    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    const int title_x = kSideInset;
    const int title_y = StatusBarHeight() + kTopGap;
    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       title_x,
                       title_y,
                       state.title_text,
                       kTitleRole,
                       design::color::kBlack);

    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       title_x,
                       layout.wifi_toggle.y - kNetworkHeadingGap - LineHeight(kSectionRole),
                       "Network",
                       kSectionRole,
                       design::color::kBlack);

    MenuToggleStyle wifi_style = {};
    wifi_style.width = layout.wifi_toggle.width;
    wifi_style.height = layout.wifi_toggle.height;
    DrawMenuToggle(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   layout.wifi_toggle.x,
                   layout.wifi_toggle.y,
                   state.wifi_toggle,
                   wifi_style);

    MenuToggleStyle access_point_style = wifi_style;
    access_point_style.bottom_border_thickness = 0;
    DrawMenuToggle(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   layout.access_point_toggle.x,
                   layout.access_point_toggle.y,
                   state.access_point_toggle,
                   access_point_style);

    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       title_x,
                       layout.storage_status.y - kStorageStatusGap - LineHeight(kSectionRole),
                       "Storage",
                       kSectionRole,
                       design::color::kBlack);

    SdStatusStyle storage_style = {};
    storage_style.max_width = layout.storage_status.width;
    DrawSdStatus(framebuffer,
                 raw_width,
                 raw_height,
                 portrait_width,
                 portrait_height,
                 layout.storage_status.x,
                 layout.storage_status.y,
                 state.storage_status,
                 storage_style);

    ButtonStyle format_button_style = {};
    format_button_style.width = layout.format_sd_button.width;
    DrawButton(framebuffer,
               raw_width,
               raw_height,
               portrait_width,
               portrait_height,
               layout.format_sd_button.x,
               layout.format_sd_button.y,
               state.format_sd_button,
               format_button_style);

    DrawGlobalFooter(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
