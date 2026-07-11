#include "epaper_ui/sticky_note.h"

#include <algorithm>
#include <array>
#include <string>

#include "epaper_ui/font_renderer.h"
#include "epaper_ui/scroll_container.h"
#include "epaper_ui/status_bar.h"
#include "generated_epaper_icons.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

// Footer controls in draw / focus order: Close, Prev (chevron-left), Next (chevron-right).
const std::array<const EmbeddedImageAsset*, kStickyNoteControlCount> kControlIcons = {
    &epaper_icons::kClose,
    &epaper_icons::kChevronLeft,
    &epaper_icons::kChevronRight,
};
constexpr std::array<StickyNoteControl, kStickyNoteControlCount> kControlSelections = {
    StickyNoteControl::kClose,
    StickyNoteControl::kPrev,
    StickyNoteControl::kNext,
};

int FooterButtonSize(const StickyNoteStyle& style)
{
    return ClampPositive(style.footer_button.size);
}

int FooterHeight(const StickyNoteStyle& style)
{
    return std::max(ClampPositive(style.footer_icon_slot_size), FooterButtonSize(style));
}

int HeaderHeight(const StickyNoteStyle& style)
{
    return std::max({ClampPositive(style.header.height), ClampPositive(style.header.icon_slot_size),
                     LineHeight(style.header.role)});
}

struct Layout {
    UiRect panel = {};
    UiRect content = {};  // body area (inside padding, above the footer row)
    int footer_y = 0;
    std::array<UiRect, kStickyNoteControlCount> controls = {};  // close, prev, next
    int counter_x = 0;
    int counter_y = 0;
};

Layout ComputeLayout(int portrait_width, int portrait_height, const StickyNoteStyle& style)
{
    Layout layout = {};

    const int margin = ClampPositive(style.screen_margin);
    const int top = StatusBarHeight() + margin;
    layout.panel = {margin, top, std::max(0, portrait_width - (2 * margin)),
                    std::max(0, portrait_height - top - margin)};

    const int pad = ClampPositive(style.padding);
    const UiRect inner = {layout.panel.x + pad, layout.panel.y + pad,
                          std::max(0, layout.panel.width - (2 * pad)),
                          std::max(0, layout.panel.height - (2 * pad))};

    const int footer_height = FooterHeight(style);
    layout.footer_y = inner.bottom() - footer_height;

    // Content is everything above the footer row, minus the footer gap.
    const int content_bottom = layout.footer_y - ClampPositive(style.footer_gap);
    layout.content = {inner.x, inner.y, inner.width, std::max(0, content_bottom - inner.y)};

    // Footer buttons, right-aligned in carousel order: [Close] <prev> <next>.
    const int button = FooterButtonSize(style);
    const int gap = ClampPositive(style.footer_button_gap);
    const int group_width = (kStickyNoteControlCount * button) +
                            ((kStickyNoteControlCount - 1) * gap);
    const int group_x = inner.right() - group_width;
    const int button_y = layout.footer_y + CenterOffset(footer_height, button);
    for (int index = 0; index < kStickyNoteControlCount; ++index) {
        layout.controls[index] = {group_x + (index * (button + gap)), button_y, button, button};
    }

    // "N/M Stickies" counter, bottom-left, vertically centered to the button row.
    layout.counter_x = inner.x;
    layout.counter_y = layout.footer_y + CenterOffset(footer_height, LineHeight(style.counter_role));
    return layout;
}

std::string CounterText(const StickyNoteState& state)
{
    const int total = std::max(0, state.sticky_count);
    const int current = total > 0 ? std::clamp(state.active_index, 0, total - 1) + 1 : 0;
    return std::to_string(current) + "/" + std::to_string(total) + " Stickies";
}

// The transcript scroll-container region: below the tag pill + header (with the header/body gap),
// filling the rest of the content area above the footer.
UiRect BodyRegion(const Layout& layout, const StickyNoteState& state, const StickyNoteStyle& style)
{
    int top = layout.content.y;
    if (!state.date_text.empty()) {
        top += LineHeight(style.date_role) + ClampPositive(style.date_header_gap);
    }
    top += HeaderHeight(style) + ClampPositive(style.header_body_gap);
    return {layout.content.x, top, layout.content.width,
            std::max(0, layout.content.bottom() - top)};
}

// Borderless scroll container that blends into the sticky surface (only text, scrollbar and focus
// ring show), left-aligned with the header (no content padding).
ScrollContainerStyle BodyScrollStyle(const UiRect& body, const StickyNoteStyle& style)
{
    ScrollContainerStyle sc = {};
    sc.width = body.width;
    sc.panel_height = body.height;
    sc.text_role = style.body_role;
    sc.text_color = style.body_color;
    sc.panel_border_thickness = 0;
    sc.content_padding = 0;
    sc.panel_background_color = style.background_color;
    sc.focus_gap_color = style.background_color;
    sc.scrollbar_track_color = style.background_color;
    return sc;
}

ScrollContainerState BodyScrollState(const StickyNoteState& state)
{
    ScrollContainerState sc = {};
    sc.content_text = state.body_text;
    sc.focused = state.scroll_focused;
    sc.active = state.scroll_active;
    sc.scroll_position_percent = state.scroll_position_percent;
    return sc;
}

}  // namespace

UiRect StickyNotePanelBounds(int portrait_width, int portrait_height, const StickyNoteStyle& style)
{
    return ComputeLayout(portrait_width, portrait_height, style).panel;
}

UiRect StickyNoteContentBounds(int portrait_width,
                               int portrait_height,
                               const StickyNoteStyle& style)
{
    return ComputeLayout(portrait_width, portrait_height, style).content;
}

UiRect StickyNoteBodyBounds(int portrait_width,
                            int portrait_height,
                            const StickyNoteState& state,
                            const StickyNoteStyle& style)
{
    const Layout layout = ComputeLayout(portrait_width, portrait_height, style);
    return BodyRegion(layout, state, style);
}

StickyNoteControlRects StickyNoteControlBounds(int portrait_width,
                                               int portrait_height,
                                               const StickyNoteStyle& style)
{
    const Layout layout = ComputeLayout(portrait_width, portrait_height, style);
    return {layout.controls[0], layout.controls[1], layout.controls[2]};
}

bool HitTestStickyNoteControl(int portrait_width,
                              int portrait_height,
                              const StickyNoteState& state,
                              const StickyNoteStyle& style,
                              int x,
                              int y,
                              StickyNoteControl* control)
{
    if (control != nullptr) {
        *control = StickyNoteControl::kNone;
    }
    if (!state.visible) {
        return false;
    }
    const Layout layout = ComputeLayout(portrait_width, portrait_height, style);
    for (int index = 0; index < kStickyNoteControlCount; ++index) {
        if (layout.controls[index].Contains(x, y)) {
            if (control != nullptr) {
                *control = kControlSelections[index];
            }
            return true;
        }
    }
    return false;
}

void DrawStickyNote(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    const StickyNoteState& state,
                    const StickyNoteStyle& style)
{
    if (!state.visible || framebuffer == nullptr) {
        return;
    }

    const Layout layout = ComputeLayout(portrait_width, portrait_height, style);
    const UiRect panel = layout.panel;
    if (panel.IsEmpty()) {
        return;
    }
    const int radius = ClampPositive(style.corner_radius);

    // Drop shadow (modal style), then the vibe-card-styled panel.
    const int shadow_offset = ClampPositive(style.shadow_offset);
    if (shadow_offset > 0) {
        const UiRect shadow = {panel.x + shadow_offset, panel.y + shadow_offset, panel.width,
                               panel.height};
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                shadow, radius, style.shadow_color);
    }
    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            panel, radius, style.background_color);
    if (style.border_thickness > 0) {
        DrawRoundedPortraitBorder(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, panel, radius,
                                  ClampPositive(style.border_thickness), style.border_color);
    }

    const UiRect content = layout.content;
    int cursor_y = content.y;

    if (!state.date_text.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           content.x, cursor_y, state.date_text, style.date_role, style.date_color);
        cursor_y += LineHeight(style.date_role) + ClampPositive(style.date_header_gap);
    }

    ListItemHeaderStyle header_style = style.header;
    header_style.width = content.width;
    DrawListItemHeader(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       content.x, cursor_y, state.header, header_style);

    // Transcript in a (borderless) scroll container -- scrolls when it overflows.
    const UiRect body = BodyRegion(layout, state, style);
    if (!body.IsEmpty()) {
        DrawScrollContainer(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            body.x, body.y, BodyScrollState(state), BodyScrollStyle(body, style));
    }

    // Footer: counter on the left, controls on the right.
    const std::string counter = CounterText(state);
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       layout.counter_x, layout.counter_y, counter, style.counter_role,
                       style.counter_color);

    for (int index = 0; index < kStickyNoteControlCount; ++index) {
        const UiRect& bounds = layout.controls[index];
        const ButtonIconState button_state = {
            .asset = kControlIcons[index],
            .selected = state.selected_control == kControlSelections[index],
        };
        DrawButtonIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height, bounds.x,
                       bounds.y, button_state, style.footer_button);
    }
}

}  // namespace epaper_ui
