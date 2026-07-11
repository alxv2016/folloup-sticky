#include "epaper_ui/card_modal.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/button.h"
#include "epaper_ui/font_renderer.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr design::TypographyRole kTitleRole = design::TypographyRole::kLabelMediumBlack;
constexpr design::TypographyRole kBodyRole = design::TypographyRole::kBody;
constexpr design::TypographyRole kButtonRole = design::TypographyRole::kLabelSmall;
constexpr int kMaxActions = 2;

int ClampedActionCount(const CardModalState& state)
{
    return std::min(static_cast<int>(state.action_labels.size()), kMaxActions);
}

std::array<std::string_view, 4> WrapLines(std::string_view text, int max_width, int* line_count)
{
    std::array<std::string_view, 4> lines = {};
    if (line_count != nullptr) {
        *line_count = 0;
    }
    if (line_count == nullptr || text.empty() || max_width <= 0) {
        return lines;
    }

    size_t line_start = 0;
    while (line_start < text.size() && *line_count < static_cast<int>(lines.size())) {
        while (line_start < text.size() && text[line_start] == ' ') {
            ++line_start;
        }
        if (line_start >= text.size()) {
            break;
        }

        size_t segment_end = line_start;
        size_t last_fit_end = line_start;
        while (segment_end < text.size()) {
            size_t next_space = text.find(' ', segment_end);
            if (next_space == std::string_view::npos) {
                next_space = text.size();
            }

            const std::string_view candidate = text.substr(line_start, next_space - line_start);
            if (MeasureText(kBodyRole, candidate) > max_width) {
                break;
            }

            last_fit_end = next_space;
            if (next_space == text.size()) {
                segment_end = next_space;
                break;
            }
            segment_end = next_space + 1;
        }

        if (last_fit_end == line_start) {
            last_fit_end = text.size();
            for (size_t split = line_start + 1; split <= text.size(); ++split) {
                if (MeasureText(kBodyRole, text.substr(line_start, split - line_start)) >
                    max_width) {
                    last_fit_end = split - 1;
                    break;
                }
            }
            if (last_fit_end <= line_start) {
                last_fit_end = std::min(text.size(), line_start + 1);
            }
        }

        lines[static_cast<size_t>(*line_count)] =
            text.substr(line_start, last_fit_end - line_start);
        ++(*line_count);
        line_start = last_fit_end;
        while (line_start < text.size() && text[line_start] == ' ') {
            ++line_start;
        }
    }

    return lines;
}

int CardHeight(int portrait_width, const CardModalState& state)
{
    const int card_width = std::max(0, portrait_width - (2 * design::modal::kScreenInset));
    const int inner_width =
        std::max(0, card_width - (2 * design::modal::kHorizontalPadding));
    const int title_height = LineHeight(kTitleRole);

    int body_line_count = 0;
    (void)WrapLines(state.body_text, inner_width, &body_line_count);
    const int body_height = body_line_count * LineHeight(kBodyRole);
    const int action_count = ClampedActionCount(state);
    const int action_height = action_count > 0 ? design::button::kHeight : 0;
    const int action_gap = action_count > 0 ? design::modal::kContentActionGap : 0;

    return design::modal::kTopPadding + title_height + design::modal::kTitleContentGap +
           body_height + action_gap + action_height + design::modal::kBottomPadding;
}

ButtonStyle BuildButtonStyle(int width)
{
    ButtonStyle style = {};
    style.variant = ButtonVariant::kDefault;
    style.role = kButtonRole;
    style.width = width;
    style.center_label = true;
    return style;
}

UiRect ActionBoundsFromCard(const UiRect& card, const CardModalState& state, int action_index)
{
    const int action_count = ClampedActionCount(state);
    if (action_index < 0 || action_index >= action_count) {
        return {};
    }

    const int inner_x = card.x + design::modal::kHorizontalPadding;
    const int inner_width = card.width - (2 * design::modal::kHorizontalPadding);
    const int y = card.bottom() - design::modal::kBottomPadding - design::button::kHeight;

    if (action_count == 1) {
        ButtonStyle style = BuildButtonStyle(inner_width);
        ButtonState button_state = {
            .label_text = state.action_labels[0],
            .selected = state.selected_action_index == 0,
        };
        return ButtonBounds(inner_x, y, button_state, style);
    }

    const int gap = design::modal::kActionGap;
    const int button_width = std::max(0, (inner_width - gap) / 2);
    const int x = action_index == 0 ? inner_x : inner_x + button_width + gap;
    ButtonStyle style = BuildButtonStyle(button_width);
    ButtonState button_state = {
        .label_text = state.action_labels[static_cast<size_t>(action_index)],
        .selected = state.selected_action_index == action_index,
    };
    return ButtonBounds(x, y, button_state, style);
}

bool HitActionAnyOrientation(int portrait_width,
                             int portrait_height,
                             const CardModalState& state,
                             int x,
                             int y,
                             int* action_index)
{
    const int action_count = ClampedActionCount(state);
    for (int index = 0; index < action_count; ++index) {
        const UiRect bounds =
            CardModalActionBounds(portrait_width, portrait_height, state, index);
        if (bounds.Contains(x, y)) {
            if (action_index != nullptr) {
                *action_index = index;
            }
            return true;
        }
    }

    const int mirrored_x = std::max(0, portrait_width - 1 - x);
    const int mirrored_y = std::max(0, portrait_height - 1 - y);
    for (int index = 0; index < action_count; ++index) {
        const UiRect bounds =
            CardModalActionBounds(portrait_width, portrait_height, state, index);
        if (bounds.Contains(mirrored_x, y) || bounds.Contains(x, mirrored_y) ||
            bounds.Contains(mirrored_x, mirrored_y)) {
            if (action_index != nullptr) {
                *action_index = index;
            }
            return true;
        }
    }

    return false;
}

}  // namespace

int CardModalActionCount(const CardModalState& state)
{
    return ClampedActionCount(state);
}

UiRect CardModalPanelBounds(int portrait_width,
                            int portrait_height,
                            const CardModalState& state)
{
    if (!state.visible) {
        return {};
    }

    const int width = std::max(0, portrait_width - (2 * design::modal::kScreenInset));
    const int height = CardHeight(portrait_width, state);
    return {
        design::modal::kScreenInset,
        std::max(design::modal::kScreenInset, (portrait_height - height) / 2),
        width,
        height,
    };
}

UiRect CardModalActionBounds(int portrait_width,
                             int portrait_height,
                             const CardModalState& state,
                             int action_index)
{
    return ActionBoundsFromCard(
        CardModalPanelBounds(portrait_width, portrait_height, state), state, action_index);
}

int HitTestCardModalAction(int portrait_width,
                           int portrait_height,
                           const CardModalState& state,
                           int x,
                           int y,
                           bool* hit)
{
    if (hit != nullptr) {
        *hit = false;
    }
    if (!state.visible) {
        return -1;
    }
    int action_index = -1;
    const bool did_hit =
        HitActionAnyOrientation(portrait_width, portrait_height, state, x, y, &action_index);
    if (hit != nullptr) {
        *hit = did_hit;
    }
    return did_hit ? action_index : -1;
}

void DrawCardModal(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const CardModalState& state)
{
    if (!state.visible || framebuffer == nullptr) {
        return;
    }

    const UiRect card = CardModalPanelBounds(portrait_width, portrait_height, state);
    if (card.IsEmpty()) {
        return;
    }

    const UiRect shadow = {
        card.x + design::modal::kShadowOffset,
        card.y + design::modal::kShadowOffset,
        card.width,
        card.height,
    };
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     shadow,
                     design::color::kShadow);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     card,
                     design::color::kWhite);
    DrawPortraitBorder(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       card,
                       design::modal::kBorderThickness,
                       design::color::kBlack);

    const int inner_x = card.x + design::modal::kHorizontalPadding;
    const int inner_width =
        std::max(0, card.width - (2 * design::modal::kHorizontalPadding));
    const int title_y = card.y + design::modal::kTopPadding;
    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       inner_x,
                       title_y,
                       state.title_text,
                       kTitleRole,
                       design::color::kBlack);

    int body_line_count = 0;
    const auto body_lines = WrapLines(state.body_text, inner_width, &body_line_count);
    int body_y = title_y + LineHeight(kTitleRole) + design::modal::kTitleContentGap;
    for (int index = 0; index < body_line_count; ++index) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           inner_x,
                           body_y,
                           body_lines[static_cast<size_t>(index)],
                           kBodyRole,
                           design::color::kBlack);
        body_y += LineHeight(kBodyRole);
    }

    const int action_count = ClampedActionCount(state);
    for (int index = 0; index < action_count; ++index) {
        const UiRect bounds =
            CardModalActionBounds(portrait_width, portrait_height, state, index);
        ButtonStyle style = BuildButtonStyle(bounds.width);
        ButtonState button_state = {
            .label_text = state.action_labels[static_cast<size_t>(index)],
            .selected = state.selected_action_index == index,
        };
        DrawButton(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   bounds.x,
                   bounds.y,
                   button_state,
                   style);
    }
}

}  // namespace epaper_ui
