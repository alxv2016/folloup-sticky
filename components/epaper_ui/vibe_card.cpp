#include "epaper_ui/vibe_card.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "asset_types.h"
#include "generated_epaper_icons.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kCornerRadius = 4;

// The three fixed footer actions, in focus order (Listen is intentionally absent).
const std::array<const EmbeddedImageAsset*, kVibeCardActionCount> kActionIcons = {
    &epaper_icons::kRefresh,
    &epaper_icons::kClose,
    &epaper_icons::kCheck,
};
constexpr std::array<VibeCardActionSelection, kVibeCardActionCount> kActionSelections = {
    VibeCardActionSelection::kRefresh,
    VibeCardActionSelection::kClose,
    VibeCardActionSelection::kCheck,
};

UiRect ExpandRect(const UiRect& rect, int amount)
{
    return {rect.x - amount, rect.y - amount, rect.width + (2 * amount), rect.height + (2 * amount)};
}

int ResolveWidth(int origin_x, int portrait_width, const VibeCardStyle& style)
{
    if (style.width > 0) {
        return style.width;
    }
    return std::max(0, portrait_width - origin_x);
}

int FocusPadding(const VibeCardStyle& style)
{
    return ClampPositive(style.focus_ring_thickness) + ClampPositive(style.focus_gap);
}

int HeaderHeight(const VibeCardStyle& style)
{
    return std::max({ClampPositive(style.header.height), ClampPositive(style.header.icon_slot_size),
                     LineHeight(style.header.role)});
}

int FooterButtonSize(const VibeCardStyle& style)
{
    return ClampPositive(style.footer_button.size);
}

int FooterHeight(const VibeCardStyle& style)
{
    return std::max(ClampPositive(style.footer_icon_slot_size), FooterButtonSize(style));
}

// Split on hard newlines, then greedily word-wrap each paragraph to `max_width`.
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

int StackHeight(int line_count, int line_height, int line_gap)
{
    if (line_count <= 0) {
        return 0;
    }
    return (line_count * line_height) + ((line_count - 1) * ClampPositive(line_gap));
}

struct VibeCardLayout {
    UiRect bounds = {};
    UiRect content = {};
    int footer_y = 0;
    bool has_actions = false;
    std::array<UiRect, kVibeCardActionCount> actions = {};
    bool has_transcribe = false;
    UiRect transcribe_action = {};
};

VibeCardLayout ComputeLayout(int origin_x,
                             int origin_y,
                             int portrait_width,
                             const VibeCardState& state,
                             const VibeCardStyle& style)
{
    VibeCardLayout layout = {};
    const int width = ResolveWidth(origin_x, portrait_width, style);
    const int padding = ClampPositive(style.padding);
    const int content_width = std::max(0, width - (2 * padding));

    int content_height = 0;
    if (state.empty) {
        const int icon_size =
            state.empty_state_icon_asset != nullptr ? ClampPositive(style.empty_state_icon_size) : 0;
        const std::vector<std::string> empty_lines =
            WrapBodyLines(style.empty_state_body_role, state.empty_state_message, content_width);
        const int empty_height = StackHeight(static_cast<int>(empty_lines.size()),
                                             LineHeight(style.empty_state_body_role),
                                             style.body_line_gap);
        content_height = icon_size +
                         (empty_height > 0 && icon_size > 0 ? ClampPositive(style.empty_state_gap)
                                                            : 0) +
                         empty_height;
    } else {
        const int tag_height =
            state.tag_text.empty()
                ? 0
                : TagBounds(0, 0, {.label_text = state.tag_text}, style.tag).height;
        const int header_height = HeaderHeight(style);
        const std::vector<std::string> body_lines =
            WrapBodyLines(style.body_role, state.body_text, content_width);
        const int body_height = StackHeight(static_cast<int>(body_lines.size()),
                                            LineHeight(style.body_role), style.body_line_gap);
        const int footer_height = FooterHeight(style);

        if (tag_height > 0) {
            content_height += tag_height;
        }
        if (tag_height > 0 && header_height > 0) {
            content_height += ClampPositive(style.tag_header_gap);
        }
        if (header_height > 0) {
            content_height += header_height;
        }
        if (header_height > 0 && body_height > 0) {
            content_height += ClampPositive(style.header_body_gap);
        }
        if (body_height > 0) {
            content_height += body_height;
        }
        if ((header_height > 0 || body_height > 0) && footer_height > 0) {
            content_height += ClampPositive(style.footer_gap);
        }
        content_height += footer_height;
    }

    const int height = style.height > 0
                           ? style.height
                           : std::max(ClampPositive(style.min_height), (2 * padding) + content_height);
    layout.bounds = {origin_x, origin_y, width, height};
    layout.content = {origin_x + padding, origin_y + padding, content_width,
                      std::max(0, height - (2 * padding))};

    if (state.empty) {
        return layout;
    }

    // The action row is pinned to the bottom of the card. For a content-sized card this lands
    // exactly where a top-down walk would place it; for a stretched (fixed-height) card it stays
    // at the bottom while the body flows from the top.
    layout.footer_y = layout.content.bottom() - FooterHeight(style);

    const int button_size = FooterButtonSize(style);
    const int gap = ClampPositive(style.footer_button_gap);
    const int group_width = (kVibeCardActionCount * button_size) +
                            ((kVibeCardActionCount - 1) * gap);
    const int group_x = layout.content.right() - group_width;
    const int button_y = layout.footer_y + CenterOffset(FooterHeight(style), button_size);
    for (int index = 0; index < kVibeCardActionCount; ++index) {
        layout.actions[index] = {group_x + (index * (button_size + gap)), button_y, button_size,
                                 button_size};
    }
    layout.has_actions = true;

    // Transcribe Star: bottom-left of the card, on the same row as the action buttons (which
    // are right-aligned), so all buttons share one baseline.
    if (state.show_transcribe_action) {
        layout.has_transcribe = true;
        layout.transcribe_action = {layout.content.x, button_y, button_size, button_size};
    }
    return layout;
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

}  // namespace

UiRect VibeCardBounds(int origin_x,
                      int origin_y,
                      int portrait_width,
                      const VibeCardState& state,
                      const VibeCardStyle& style)
{
    return ComputeLayout(origin_x, origin_y, portrait_width, state, style).bounds;
}

bool HitTestVibeCardAction(int origin_x,
                           int origin_y,
                           int portrait_width,
                           const VibeCardState& state,
                           const VibeCardStyle& style,
                           int x,
                           int y,
                           VibeCardActionSelection* action)
{
    if (state.empty) {
        return false;
    }
    const VibeCardLayout layout = ComputeLayout(origin_x, origin_y, portrait_width, state, style);
    if (layout.has_transcribe && layout.transcribe_action.Contains(x, y)) {
        if (action != nullptr) {
            *action = VibeCardActionSelection::kTranscribe;
        }
        return true;
    }
    if (!layout.has_actions) {
        return false;
    }
    for (int index = 0; index < kVibeCardActionCount; ++index) {
        if (layout.actions[index].Contains(x, y)) {
            if (action != nullptr) {
                *action = kActionSelections[index];
            }
            return true;
        }
    }
    return false;
}

void DrawVibeCard(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const VibeCardState& state,
                  const VibeCardStyle& style)
{
    const VibeCardLayout layout = ComputeLayout(origin_x, origin_y, portrait_width, state, style);
    const UiRect bounds = layout.bounds;
    if (bounds.IsEmpty()) {
        return;
    }
    const UiRect content = layout.content;
    const uint8_t card_background =
        state.empty ? style.empty_state_background_color : style.background_color;

    if (state.focused) {
        const bool active = state.footer.selected_action != VibeCardActionSelection::kNone;
        const uint8_t ring_color =
            active ? style.active_focus_ring_color : style.inactive_focus_ring_color;
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                ExpandRect(bounds, FocusPadding(style)),
                                kCornerRadius + FocusPadding(style), ring_color);
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                ExpandRect(bounds, ClampPositive(style.focus_gap)),
                                kCornerRadius + ClampPositive(style.focus_gap),
                                style.focus_gap_color);
    }

    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            bounds, kCornerRadius, card_background);
    if (style.border_thickness > 0) {
        DrawRoundedPortraitBorder(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, bounds, kCornerRadius,
                                  ClampPositive(style.border_thickness), style.border_color);
    }

    if (state.empty) {
        const int icon_size =
            state.empty_state_icon_asset != nullptr ? ClampPositive(style.empty_state_icon_size) : 0;
        const std::vector<std::string> empty_lines =
            WrapBodyLines(style.empty_state_body_role, state.empty_state_message, content.width);
        const int empty_height = StackHeight(static_cast<int>(empty_lines.size()),
                                             LineHeight(style.empty_state_body_role),
                                             style.body_line_gap);
        const int stack_height =
            icon_size +
            (empty_height > 0 && icon_size > 0 ? ClampPositive(style.empty_state_gap) : 0) +
            empty_height;
        int cursor_y = content.y + CenterOffset(content.height, stack_height);
        const bool outline = style.empty_state_background_color == design::color::kGrayLight;

        if (state.empty_state_icon_asset != nullptr && icon_size > 0) {
            const int icon_x = content.x + CenterOffset(content.width, icon_size);
            const auto draw_icon = [&](int px, int py, uint8_t tone) {
                DrawScaledPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width,
                                            portrait_height, {px, py, icon_size, icon_size},
                                            state.empty_state_icon_asset, tone);
            };
            if (outline) {
                DrawOutlinedText(icon_x, cursor_y, style.body_outline_thickness, draw_icon);
            }
            draw_icon(icon_x, cursor_y, design::color::kBlack);
            cursor_y += icon_size;
        }

        if (!empty_lines.empty()) {
            if (icon_size > 0) {
                cursor_y += ClampPositive(style.empty_state_gap);
            }
            const int line_height = LineHeight(style.empty_state_body_role);
            for (const std::string& line : empty_lines) {
                const int line_width = MeasureText(style.empty_state_body_role, line);
                const int line_x = content.x + CenterOffset(content.width, line_width);
                const auto draw_line = [&](int px, int py, uint8_t tone) {
                    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                       portrait_height, px, py, line, style.empty_state_body_role,
                                       tone);
                };
                if (outline) {
                    DrawOutlinedText(line_x, cursor_y, style.body_outline_thickness, draw_line);
                }
                draw_line(line_x, cursor_y, style.empty_state_body_color);
                cursor_y += line_height + ClampPositive(style.body_line_gap);
            }
        }
        return;
    }

    int cursor_y = content.y;

    if (!state.tag_text.empty()) {
        const TagState tag_state = {.label_text = state.tag_text};
        DrawTag(framebuffer, raw_width, raw_height, portrait_width, portrait_height, content.x,
                cursor_y, tag_state, style.tag);
        cursor_y += TagBounds(0, 0, tag_state, style.tag).height + ClampPositive(style.tag_header_gap);
    }

    ListItemHeaderStyle header_style = style.header;
    header_style.width = content.width;
    DrawListItemHeader(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       content.x, cursor_y, state.header, header_style);
    const int header_height = HeaderHeight(style);
    cursor_y += header_height;

    const std::vector<std::string> body_lines =
        WrapBodyLines(style.body_role, state.body_text, content.width);
    if (!body_lines.empty()) {
        cursor_y += ClampPositive(style.header_body_gap);
        const int line_height = LineHeight(style.body_role);
        const bool outline = style.background_color != design::color::kWhite;
        // Never let the body overrun the (bottom-pinned) action row.
        const int body_limit = layout.has_actions ? layout.footer_y - ClampPositive(style.footer_gap)
                                                   : content.bottom();
        for (size_t index = 0; index < body_lines.size(); ++index) {
            if (cursor_y + line_height > body_limit) {
                break;
            }
            const std::string& line = body_lines[index];
            const auto draw_line = [&](int px, int py, uint8_t tone) {
                DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                   portrait_height, px, py, line, style.body_role, tone);
            };
            if (outline) {
                DrawOutlinedText(content.x, cursor_y, style.body_outline_thickness,
                                 [&](int px, int py, uint8_t) {
                                     draw_line(px, py, style.body_outline_color);
                                 });
            }
            draw_line(content.x, cursor_y, style.body_color);
            cursor_y += line_height;
            if (index + 1 < body_lines.size()) {
                cursor_y += ClampPositive(style.body_line_gap);
            }
        }
    }

    if (!layout.has_actions) {
        return;
    }
    for (int index = 0; index < kVibeCardActionCount; ++index) {
        const UiRect& action_bounds = layout.actions[index];
        const ButtonIconState button_state = {
            .asset = kActionIcons[index],
            .selected = state.footer.selected_action == kActionSelections[index],
        };
        DrawButtonIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       action_bounds.x, action_bounds.y, button_state, style.footer_button);
    }

    if (layout.has_transcribe) {
        const ButtonIconState star_state = {
            .asset = &epaper_icons::kStar,
            .selected = state.footer.selected_action == VibeCardActionSelection::kTranscribe,
        };
        DrawButtonIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       layout.transcribe_action.x, layout.transcribe_action.y, star_state,
                       style.footer_button);
    }
}

}  // namespace epaper_ui
