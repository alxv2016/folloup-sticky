#include "epaper_ui/global_footer.h"

#include <algorithm>
#include <array>

#include "epaper_ui/button_icon.h"
#include "epaper_ui/layout_grid.h"
#include "epaper_ui/mic_status.h"
#include "project_assets.h"

namespace epaper_ui {
namespace {

constexpr int kColumnGap = design::spacing::k16;
constexpr int kButtonGap = design::spacing::k8;
constexpr int kTouchHitSlopX = design::spacing::k4;
constexpr int kTouchHitSlopY = design::spacing::k12;

struct VisibleFooterButton {
    GlobalFooterItemId item = GlobalFooterItemId::kNone;
    const FooterButtonState* state = nullptr;
};

const EmbeddedImageAsset* ResolveButtonIcon(const FooterButtonState& button)
{
    return button.icon;
}

const EmbeddedImageAsset* ResolveMicIdleIcon(const FooterMicState& mic)
{
    return mic.idle_icon != nullptr ? mic.idle_icon
                                    : project_assets::GetIcon(EmbeddedIconId::kMicOff);
}

const EmbeddedImageAsset* ResolveMicActiveIcon(const FooterMicState& mic)
{
    return mic.active_icon != nullptr ? mic.active_icon
                                      : project_assets::GetIcon(EmbeddedIconId::kMicOn);
}

std::array<const FooterButtonState*, 5> VisibleButtons(const GlobalFooterState& state,
                                                       int* count_out)
{
    std::array<const FooterButtonState*, 5> buttons = {};
    std::array<VisibleFooterButton, 5> mapped = {{
        {.item = GlobalFooterItemId::kSettings, .state = &state.settings},
        {.item = GlobalFooterItemId::kWifi, .state = &state.wifi},
        {.item = GlobalFooterItemId::kTime, .state = &state.time},
        {.item = GlobalFooterItemId::kFolder, .state = &state.folder},
        {.item = GlobalFooterItemId::kHome, .state = &state.home},
    }};
    int count = 0;
    for (const VisibleFooterButton& button : mapped) {
        if (button.state != nullptr && button.state->visible &&
            ResolveButtonIcon(*button.state) != nullptr) {
            buttons[static_cast<size_t>(count++)] = button.state;
        }
    }

    if (count_out != nullptr) {
        *count_out = count;
    }
    return buttons;
}

std::array<VisibleFooterButton, 5> VisibleFooterButtons(const GlobalFooterState& state,
                                                        int* count_out)
{
    std::array<VisibleFooterButton, 5> buttons = {};
    int count = 0;
    const VisibleFooterButton candidates[] = {
        {.item = GlobalFooterItemId::kSettings, .state = &state.settings},
        {.item = GlobalFooterItemId::kWifi, .state = &state.wifi},
        {.item = GlobalFooterItemId::kTime, .state = &state.time},
        {.item = GlobalFooterItemId::kFolder, .state = &state.folder},
        {.item = GlobalFooterItemId::kHome, .state = &state.home},
    };

    for (const VisibleFooterButton& button : candidates) {
        if (button.state != nullptr && button.state->visible &&
            ResolveButtonIcon(*button.state) != nullptr) {
            buttons[static_cast<size_t>(count++)] = button;
        }
    }

    if (count_out != nullptr) {
        *count_out = count;
    }
    return buttons;
}

LayoutGrid BuildGrid(int portrait_width, int portrait_height)
{
    const int y = std::max(0,
                           portrait_height - design::global_footer::kBottomPadding -
                               design::global_footer::kButtonSize);

    LayoutGrid grid = {};
    grid.origin_x = design::spacing::k16;
    grid.origin_y = y;
    grid.style.width = std::max(0, portrait_width - (2 * design::spacing::k16));
    grid.style.min_height = design::global_footer::kButtonSize;
    grid.style.column_count = 2;
    grid.style.row_count = 1;
    grid.style.padding_left = 0;
    grid.style.padding_right = 0;
    grid.style.padding_top = 0;
    grid.style.padding_bottom = 0;
    grid.style.column_gap = kColumnGap;
    grid.style.row_gap = 0;
    grid.style.fixed_column_side = FixedColumnSide::kLeft;
    grid.style.fixed_column_width = (design::global_footer::kButtonSize * 5) + (kButtonGap * 4);
    return grid;
}

MicStatusStyle BuildMicStyle()
{
    MicStatusStyle style = {};
    style.size = design::global_footer::kButtonSize;
    style.icon_size = design::global_footer::kIconSize;
    style.border_thickness = 0;
    style.outline_idle_icon = false;
    return style;
}

ButtonIconStyle BuildFooterButtonStyle()
{
    ButtonIconStyle style = {};
    style.background_color = design::status_bar::kBackgroundColor;
    style.selected_background_color = design::color::kBlack;
    style.border_thickness = 0;
    style.size = design::global_footer::kButtonSize;
    style.icon_size = design::global_footer::kIconSize;
    style.outline_icon_when_unselected = true;
    return style;
}

UiRect MicBounds(int portrait_width, int portrait_height, const GlobalFooterState& state)
{
    const LayoutGrid grid = BuildGrid(portrait_width, portrait_height);
    const UiRect status_cell = grid.CellBounds(portrait_width, 1, 0);
    const MicStatusStyle style = BuildMicStyle();
    const MicStatusState mic_state = {
        .label_text = {},
        .focused = state.mic.selected || state.mic.active,
    };
    const UiRect measured = MicStatusBounds(0, 0, mic_state, style);
    return {std::max(status_cell.x, status_cell.right() - measured.width),
            status_cell.y + std::max(0, (status_cell.height - measured.height) / 2),
            std::min(measured.width, status_cell.width),
            measured.height};
}

UiRect FooterButtonBounds(int portrait_width,
                          int portrait_height,
                          const GlobalFooterState& state,
                          GlobalFooterItemId item)
{
    if (item == GlobalFooterItemId::kNone || item == GlobalFooterItemId::kMic) {
        return {};
    }

    const LayoutGrid grid = BuildGrid(portrait_width, portrait_height);
    const UiRect button_cell = grid.CellBounds(portrait_width, 0, 0);
    const ButtonIconStyle button_style = BuildFooterButtonStyle();

    int button_count = 0;
    const std::array<VisibleFooterButton, 5> buttons = VisibleFooterButtons(state, &button_count);
    int cursor_x = button_cell.x;
    for (int index = 0; index < button_count; ++index) {
        const VisibleFooterButton& button = buttons[static_cast<size_t>(index)];
        if (button.state == nullptr) {
            continue;
        }

        const UiRect bounds = ButtonIconBounds(
            cursor_x,
            button_cell.y + std::max(0, (button_cell.height - button_style.size) / 2),
            button_style);
        if (button.item == item) {
            return bounds;
        }
        cursor_x += design::global_footer::kButtonSize + kButtonGap;
    }

    return {};
}

UiRect ExpandTouchBounds(const UiRect& bounds)
{
    if (bounds.IsEmpty()) {
        return bounds;
    }

    return {
        bounds.x - kTouchHitSlopX,
        bounds.y - kTouchHitSlopY,
        bounds.width + (2 * kTouchHitSlopX),
        bounds.height + (2 * kTouchHitSlopY),
    };
}

}  // namespace

UiRect GlobalFooterBounds(int portrait_width, int portrait_height, const GlobalFooterState& state)
{
    if (!state.visible) {
        return {};
    }

    return BuildGrid(portrait_width, portrait_height).Measure(portrait_width);
}

UiRect GlobalFooterItemBounds(int portrait_width,
                              int portrait_height,
                              const GlobalFooterState& state,
                              GlobalFooterItemId item)
{
    if (!state.visible) {
        return {};
    }

    if (item == GlobalFooterItemId::kMic) {
        return state.mic.visible ? MicBounds(portrait_width, portrait_height, state) : UiRect{};
    }

    return FooterButtonBounds(portrait_width, portrait_height, state, item);
}

UiRect GlobalFooterItemVisualBounds(int portrait_width,
                                    int portrait_height,
                                    const GlobalFooterState& state,
                                    GlobalFooterItemId item)
{
    return GlobalFooterItemBounds(portrait_width, portrait_height, state, item);
}

bool HitTestGlobalFooterItem(int portrait_width,
                             int portrait_height,
                             const GlobalFooterState& state,
                             int x,
                             int y,
                             GlobalFooterItemId* item)
{
    if (item != nullptr) {
        *item = GlobalFooterItemId::kNone;
    }
    if (!state.visible) {
        return false;
    }

    constexpr GlobalFooterItemId kItems[] = {
        GlobalFooterItemId::kSettings,
        GlobalFooterItemId::kWifi,
        GlobalFooterItemId::kTime,
        GlobalFooterItemId::kFolder,
        GlobalFooterItemId::kHome,
        GlobalFooterItemId::kMic,
    };

    for (GlobalFooterItemId candidate : kItems) {
        const UiRect bounds =
            ExpandTouchBounds(GlobalFooterItemBounds(portrait_width, portrait_height, state, candidate));
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (item != nullptr) {
                *item = candidate;
            }
            return true;
        }
    }

    return false;
}

void DrawGlobalFooter(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const GlobalFooterState& state)
{
    const UiRect bounds = GlobalFooterBounds(portrait_width, portrait_height, state);
    if (bounds.IsEmpty()) {
        return;
    }

    const LayoutGrid grid = BuildGrid(portrait_width, portrait_height);
    const UiRect button_cell = grid.CellBounds(portrait_width, 0, 0);
    const ButtonIconStyle button_style = BuildFooterButtonStyle();

    int button_count = 0;
    const std::array<const FooterButtonState*, 5> buttons = VisibleButtons(state, &button_count);
    int cursor_x = button_cell.x;
    for (int index = 0; index < button_count; ++index) {
        const FooterButtonState* button = buttons[static_cast<size_t>(index)];
        if (button == nullptr) {
            continue;
        }

        DrawButtonIcon(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       cursor_x,
                       button_cell.y + std::max(0, (button_cell.height - button_style.size) / 2),
                       {.asset = ResolveButtonIcon(*button), .selected = button->selected},
                       button_style);
        cursor_x += design::global_footer::kButtonSize + kButtonGap;
    }

    if (!state.mic.visible) {
        return;
    }

    const UiRect mic_bounds = MicBounds(portrait_width, portrait_height, state);
    const MicStatusStyle mic_style = BuildMicStyle();
    const MicStatusState mic_state = {
        .label_text = {},
        .focused = state.mic.selected || state.mic.active,
    };
    DrawMicStatus(framebuffer,
                  raw_width,
                  raw_height,
                  portrait_width,
                  portrait_height,
                  mic_bounds.x,
                  mic_bounds.y,
                  mic_state,
                  mic_style,
                  ResolveMicIdleIcon(state.mic),
                  ResolveMicActiveIcon(state.mic));
}

}  // namespace epaper_ui
