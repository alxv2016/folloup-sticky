#include "epaper_ui/wifi_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kHeadingBottomPadding = design::spacing::k24;
constexpr int kSectionGap = design::spacing::k16;
constexpr int kPasswordActionGap = design::spacing::k16;
constexpr int kActionButtonGap = design::spacing::k12;
constexpr int kFooterButtonGap = design::spacing::k32;

struct Layout {
    UiRect network_list = {};
    UiRect password_input = {};
    UiRect scan_button = {};
    UiRect connect_button = {};
};

NetworkListStyle BuildNetworkListStyle(int width, int panel_height)
{
    NetworkListStyle style = {};
    style.width = width;
    style.panel_height = panel_height;
    style.scrollbar_gap = 0;
    return style;
}

Layout BuildLayout(int portrait_width, int portrait_height, const WifiPageState& state)
{
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_bottom = title_y + LineHeight(kTitleRole);
    const int footer_top = portrait_height - design::global_footer::kBottomPadding -
                           design::global_footer::kButtonSize;
    const int bottom_limit = std::max(title_bottom, footer_top - kFooterButtonGap);

    PasswordInputStyle password_style = {};
    password_style.width = page_width;
    ButtonStyle action_style = {};
    action_style.width = std::max(0, (page_width - kActionButtonGap) / 2);
    action_style.center_label = true;
    const UiRect measured_password_input =
        PasswordInputBounds(page_x, 0, state.password_input, password_style);
    const UiRect measured_button = ButtonBounds(page_x, 0, state.scan_button, action_style);
    const int action_y = std::max(
        title_bottom + kHeadingBottomPadding + kSectionGap + measured_password_input.height +
            kPasswordActionGap,
        bottom_limit - measured_button.height);
    const UiRect password_input = PasswordInputBounds(
        page_x,
        std::max(title_bottom + kHeadingBottomPadding + kSectionGap,
                 action_y - kPasswordActionGap - measured_password_input.height),
        state.password_input,
        password_style);

    NetworkListStyle network_style = BuildNetworkListStyle(page_width, 0);
    const int status_reserve =
        LineHeight(network_style.status_role) + network_style.section_gap;
    const int network_top = title_bottom + kHeadingBottomPadding;
    network_style.panel_height =
        std::max(0, password_input.y - kSectionGap - status_reserve - network_top);
    const UiRect network_list =
        NetworkListPanelBounds(page_x, network_top, network_style);

    const int button_width = action_style.width;
    const UiRect scan_button = ButtonBounds(page_x, action_y, state.scan_button, action_style);
    const UiRect connect_button = ButtonBounds(
        page_x + button_width + kActionButtonGap, action_y, state.connect_button, action_style);
    return {
        .network_list = network_list,
        .password_input = password_input,
        .scan_button = scan_button,
        .connect_button = connect_button,
    };
}

}  // namespace

UiRect WifiPageItemBounds(int portrait_width,
                          int portrait_height,
                          const WifiPageState& state,
                          WifiPageItemId item)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    switch (item) {
        case WifiPageItemId::kNetworkList:
            return layout.network_list;
        case WifiPageItemId::kPasswordInput:
            return layout.password_input;
        case WifiPageItemId::kPasswordVisibilityButton: {
            PasswordInputStyle style = {};
            style.width = layout.password_input.width;
            return PasswordInputVisibilityButtonBounds(layout.password_input.x,
                                                      layout.password_input.y,
                                                      state.password_input,
                                                      style);
        }
        case WifiPageItemId::kScanButton:
            return layout.scan_button;
        case WifiPageItemId::kConnectButton:
            return layout.connect_button;
        case WifiPageItemId::kNone:
        default:
            return {};
    }
}

UiRect WifiPageNetworkRowBounds(int portrait_width,
                                int portrait_height,
                                const WifiPageState& state,
                                int row_index)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    const NetworkListStyle style =
        BuildNetworkListStyle(layout.network_list.width, layout.network_list.height);
    return NetworkListRowBounds(layout.network_list.x,
                                layout.network_list.y,
                                state.network_list,
                                style,
                                row_index);
}

bool HitTestWifiPageItem(int portrait_width,
                         int portrait_height,
                         const WifiPageState& state,
                         int x,
                         int y,
                         WifiPageItemId* item)
{
    if (item != nullptr) {
        *item = WifiPageItemId::kNone;
    }

    constexpr WifiPageItemId kItems[] = {
        WifiPageItemId::kPasswordVisibilityButton,
        WifiPageItemId::kPasswordInput,
        WifiPageItemId::kScanButton,
        WifiPageItemId::kConnectButton,
        WifiPageItemId::kNetworkList,
    };
    for (WifiPageItemId candidate : kItems) {
        const UiRect bounds = WifiPageItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (item != nullptr) {
                *item = candidate;
            }
            return true;
        }
    }
    return false;
}

bool HitTestWifiPageNetworkRow(int portrait_width,
                               int portrait_height,
                               const WifiPageState& state,
                               int x,
                               int y,
                               int* row_index)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    const NetworkListStyle style =
        BuildNetworkListStyle(layout.network_list.width, layout.network_list.height);
    return HitTestNetworkListRow(layout.network_list.x,
                                 layout.network_list.y,
                                 state.network_list,
                                 style,
                                 x,
                                 y,
                                 row_index);
}

void DrawWifiPage(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  const WifiPageState& state,
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

    const NetworkListStyle network_style =
        BuildNetworkListStyle(layout.network_list.width, layout.network_list.height);
    DrawNetworkList(framebuffer,
                    raw_width,
                    raw_height,
                    portrait_width,
                    portrait_height,
                    layout.network_list.x,
                    layout.network_list.y,
                    state.network_list,
                    network_style);

    PasswordInputStyle password_style = {};
    password_style.width = layout.password_input.width;
    DrawPasswordInput(framebuffer,
                      raw_width,
                      raw_height,
                      portrait_width,
                      portrait_height,
                      layout.password_input.x,
                      layout.password_input.y,
                      state.password_input,
                      password_style);

    ButtonStyle action_style = {};
    action_style.width = layout.scan_button.width;
    action_style.center_label = true;
    DrawButton(framebuffer,
               raw_width,
               raw_height,
               portrait_width,
               portrait_height,
               layout.scan_button.x,
               layout.scan_button.y,
               state.scan_button,
               action_style);
    DrawButton(framebuffer,
               raw_width,
               raw_height,
               portrait_width,
               portrait_height,
               layout.connect_button.x,
               layout.connect_button.y,
               state.connect_button,
               {.variant = ButtonVariant::kPrimary,
                .width = layout.connect_button.width,
                .center_label = true});

    DrawGlobalFooter(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
