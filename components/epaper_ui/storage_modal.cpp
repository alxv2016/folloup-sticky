#include "epaper_ui/storage_modal.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/button.h"
#include "epaper_ui/font_renderer.h"
#include "epaper_ui/progress_bar.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr design::TypographyRole kTitleRole = design::TypographyRole::kHeadingH2;
constexpr design::TypographyRole kBodyRole = design::TypographyRole::kBody;
constexpr design::TypographyRole kButtonRole = design::TypographyRole::kLabelSmall;
constexpr int kProgressTopGap = design::spacing::k12;

std::string_view TitleForKind(StorageModalKind kind)
{
    switch (kind) {
        case StorageModalKind::kNoSdCard:
            return "No SD card";
        case StorageModalKind::kConfirmFormat:
            return "Format SD card?";
        case StorageModalKind::kFormatting:
            return "Formatting SD card";
        case StorageModalKind::kFormatSuccess:
            return "Format success";
        case StorageModalKind::kFormatError:
            return "Format failed";
        case StorageModalKind::kNone:
        default:
            return {};
    }
}

std::string_view BodyForKind(StorageModalKind kind)
{
    switch (kind) {
        case StorageModalKind::kNoSdCard:
            return "No SD card is inserted. Insert an SD card to continue.";
        case StorageModalKind::kConfirmFormat:
            return "Formatting the SD card will erase everything on the card.";
        case StorageModalKind::kFormatSuccess:
            return "The SD card was formatted successfully.";
        case StorageModalKind::kFormatError:
            return "There was an error and the SD card could not be formatted.";
        case StorageModalKind::kFormatting:
        case StorageModalKind::kNone:
        default:
            return {};
    }
}

int ActionCountForKind(StorageModalKind kind)
{
    switch (kind) {
        case StorageModalKind::kConfirmFormat:
            return 2;
        case StorageModalKind::kNoSdCard:
        case StorageModalKind::kFormatSuccess:
        case StorageModalKind::kFormatError:
            return 1;
        case StorageModalKind::kFormatting:
        case StorageModalKind::kNone:
        default:
            return 0;
    }
}

std::string_view ActionLabelForKind(StorageModalKind kind, int action_index)
{
    switch (kind) {
        case StorageModalKind::kConfirmFormat:
            return action_index == 1 ? "Format" : "Cancel";
        case StorageModalKind::kNoSdCard:
        case StorageModalKind::kFormatSuccess:
        case StorageModalKind::kFormatError:
            return "OK";
        case StorageModalKind::kFormatting:
        case StorageModalKind::kNone:
        default:
            return {};
    }
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

int CardHeight(int portrait_width, const StorageModalState& state)
{
    const int card_width = std::max(0, portrait_width - (2 * design::modal::kScreenInset));
    const int inner_width =
        std::max(0, card_width - (2 * design::modal::kHorizontalPadding));
    const int title_height = LineHeight(kTitleRole);

    if (state.kind == StorageModalKind::kFormatting) {
        ProgressBarStyle progress_style = {};
        progress_style.width = inner_width;
        const ProgressBarState progress_state = {
            .label_text = "Progress",
            .status_text = "100%",
            .progress_percent = 100,
        };
        const UiRect progress_bounds =
            ProgressBarBounds(0, 0, progress_state, progress_style);
        return design::modal::kTopPadding + title_height + kProgressTopGap +
               progress_bounds.height + design::modal::kBottomPadding;
    }

    int body_line_count = 0;
    (void)WrapLines(BodyForKind(state.kind), inner_width, &body_line_count);
    const int body_height = body_line_count * LineHeight(kBodyRole);
    const int action_count = ActionCountForKind(state.kind);
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

UiRect ActionBoundsFromCard(const UiRect& card, const StorageModalState& state, int action_index)
{
    const int action_count = ActionCountForKind(state.kind);
    if (action_index < 0 || action_index >= action_count) {
        return {};
    }

    const int inner_x = card.x + design::modal::kHorizontalPadding;
    const int inner_width = card.width - (2 * design::modal::kHorizontalPadding);
    const int y = card.bottom() - design::modal::kBottomPadding - design::button::kHeight;

    if (action_count == 1) {
        ButtonStyle style = BuildButtonStyle(inner_width);
        ButtonState button_state = {
            .label_text = ActionLabelForKind(state.kind, 0),
            .selected = state.selected_action_index == 0,
        };
        return ButtonBounds(inner_x, y, button_state, style);
    }

    const int gap = design::modal::kActionGap;
    const int button_width = std::max(0, (inner_width - gap) / 2);
    const int x = action_index == 0 ? inner_x : inner_x + button_width + gap;
    ButtonStyle style = BuildButtonStyle(button_width);
    ButtonState button_state = {
        .label_text = ActionLabelForKind(state.kind, action_index),
        .selected = state.selected_action_index == action_index,
    };
    return ButtonBounds(x, y, button_state, style);
}

bool HitActionAnyOrientation(int portrait_width,
                             int portrait_height,
                             const StorageModalState& state,
                             int x,
                             int y,
                             int* action_index)
{
    const int action_count = ActionCountForKind(state.kind);
    for (int index = 0; index < action_count; ++index) {
        const UiRect bounds = StorageModalActionBounds(
            portrait_width, portrait_height, state, index);
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
        const UiRect bounds = StorageModalActionBounds(
            portrait_width, portrait_height, state, index);
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

UiRect StorageModalPanelBounds(int portrait_width,
                               int portrait_height,
                               const StorageModalState& state)
{
    if (!state.visible || state.kind == StorageModalKind::kNone) {
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

UiRect StorageModalActionBounds(int portrait_width,
                                int portrait_height,
                                const StorageModalState& state,
                                int action_index)
{
    return ActionBoundsFromCard(
        StorageModalPanelBounds(portrait_width, portrait_height, state), state, action_index);
}

int HitTestStorageModalAction(int portrait_width,
                              int portrait_height,
                              const StorageModalState& state,
                              int x,
                              int y,
                              bool* hit)
{
    if (hit != nullptr) {
        *hit = false;
    }
    int action_index = -1;
    const bool did_hit =
        HitActionAnyOrientation(portrait_width, portrait_height, state, x, y, &action_index);
    if (hit != nullptr) {
        *hit = did_hit;
    }
    return did_hit ? action_index : -1;
}

void DrawStorageModal(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const StorageModalState& state)
{
    if (!state.visible || state.kind == StorageModalKind::kNone) {
        return;
    }

    const UiRect card = StorageModalPanelBounds(portrait_width, portrait_height, state);
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
                     design::color::kGray3);
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
                       TitleForKind(state.kind),
                       kTitleRole,
                       design::color::kBlack);

    if (state.kind == StorageModalKind::kFormatting) {
        ProgressBarStyle progress_style = {};
        progress_style.width = inner_width;
        const int progress_y = title_y + LineHeight(kTitleRole) + kProgressTopGap;
        char percent_buffer[16] = {};
        std::snprintf(percent_buffer,
                      sizeof(percent_buffer),
                      "%d%%",
                      std::clamp(state.progress_percent, 0, 100));
        const ProgressBarState progress_state = {
            .label_text = "Progress",
            .status_text = percent_buffer,
            .progress_percent = state.progress_percent,
        };
        DrawProgressBar(framebuffer,
                        raw_width,
                        raw_height,
                        portrait_width,
                        portrait_height,
                        inner_x,
                        progress_y,
                        progress_state,
                        progress_style);
        return;
    }

    int body_line_count = 0;
    const auto body_lines = WrapLines(BodyForKind(state.kind), inner_width, &body_line_count);
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

    const int action_count = ActionCountForKind(state.kind);
    for (int index = 0; index < action_count; ++index) {
        const UiRect bounds = StorageModalActionBounds(
            portrait_width, portrait_height, state, index);
        ButtonStyle style = BuildButtonStyle(bounds.width);
        ButtonState button_state = {
            .label_text = ActionLabelForKind(state.kind, index),
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
