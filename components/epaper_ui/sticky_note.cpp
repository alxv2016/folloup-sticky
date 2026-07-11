#include "epaper_ui/sticky_note.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "epaper_ui/font_renderer.h"
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

// Split on hard newlines, then greedily word-wrap each paragraph to `max_width` (mirrors the vibe
// card so the sticky note wraps identically).
std::vector<std::string> WrapBodyLines(design::TypographyRole role,
                                       const std::string& text,
                                       int max_width)
{
    std::vector<std::string> lines;
    if (text.empty() || max_width <= 0) {
        return lines;
    }
    size_t start = 0;
    while (start <= text.size()) {
        const size_t newline = text.find('\n', start);
        const std::string paragraph = text.substr(
            start, newline == std::string::npos ? std::string::npos : newline - start);
        const std::vector<std::string> wrapped = WrapTextToWidth(role, paragraph, max_width);
        if (wrapped.empty()) {
            lines.emplace_back("");
        } else {
            lines.insert(lines.end(), wrapped.begin(), wrapped.end());
        }
        if (newline == std::string::npos) {
            break;
        }
        start = newline + 1;
    }
    return lines;
}

template <typename DrawFn>
void DrawOutlinedText(int origin_x, int origin_y, int stroke_thickness, DrawFn&& draw_fn)
{
    const int thickness = ClampPositive(stroke_thickness);
    for (int dy = -thickness; dy <= thickness; ++dy) {
        for (int dx = -thickness; dx <= thickness; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            draw_fn(origin_x + dx, origin_y + dy, design::color::kWhite);
        }
    }
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

    if (!state.tag_text.empty()) {
        const TagState tag_state = {.label_text = state.tag_text};
        DrawTag(framebuffer, raw_width, raw_height, portrait_width, portrait_height, content.x,
                cursor_y, tag_state, style.tag);
        cursor_y +=
            TagBounds(0, 0, tag_state, style.tag).height + ClampPositive(style.tag_header_gap);
    }

    ListItemHeaderStyle header_style = style.header;
    header_style.width = content.width;
    DrawListItemHeader(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       content.x, cursor_y, state.header, header_style);
    cursor_y += HeaderHeight(style);

    const std::vector<std::string> body_lines =
        WrapBodyLines(style.body_role, state.body_text, content.width);
    if (!body_lines.empty()) {
        cursor_y += ClampPositive(style.header_body_gap);
        const int line_height = LineHeight(style.body_role);
        const bool outline = style.background_color != design::color::kWhite;
        for (size_t index = 0; index < body_lines.size(); ++index) {
            if (cursor_y + line_height > content.bottom()) {
                break;  // never overrun the footer row
            }
            const std::string& line = body_lines[index];
            const auto draw_line = [&](int px, int py, uint8_t tone) {
                DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                   portrait_height, px, py, line, style.body_role, tone);
            };
            if (outline) {
                DrawOutlinedText(content.x, cursor_y, design::status_bar::kStrokeThickness,
                                 [&](int px, int py, uint8_t tone) { draw_line(px, py, tone); });
            }
            draw_line(content.x, cursor_y, style.body_color);
            cursor_y += line_height;
            if (index + 1 < body_lines.size()) {
                cursor_y += ClampPositive(style.body_line_gap);
            }
        }
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
