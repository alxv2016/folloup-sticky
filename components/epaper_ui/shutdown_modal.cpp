#include "epaper_ui/shutdown_modal.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/font_renderer.h"

namespace epaper_ui {
namespace {

constexpr std::string_view kTitleText = "Shut down device?";
constexpr std::string_view kBodyText =
    "Your device will power off. Do you want to continue?";
constexpr std::string_view kCancelLabel = "Cancel";
constexpr std::string_view kConfirmLabel = "Shut down";
constexpr design::TypographyRole kTitleRole = design::TypographyRole::kHeadingH2;
constexpr design::TypographyRole kBodyRole = design::TypographyRole::kBodyLarge;
constexpr design::TypographyRole kButtonRole = design::TypographyRole::kLabelMediumBlack;

void DrawRawPixel(uint8_t* framebuffer, int raw_width, int raw_height, int x, int y, bool black)
{
    if (framebuffer == nullptr || x < 0 || y < 0 || x >= raw_width || y >= raw_height) {
        return;
    }

    const size_t index = static_cast<size_t>(y) * static_cast<size_t>(raw_width / 8) +
                         static_cast<size_t>(x / 8);
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 0x07));
    if (black) {
        framebuffer[index] &= static_cast<uint8_t>(~mask);
    } else {
        framebuffer[index] |= mask;
    }
}

void DrawPortraitPixel(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       int x,
                       int y,
                       bool black)
{
    if (x < 0 || y < 0 || x >= portrait_width || y >= portrait_height) {
        return;
    }

    const int raw_x = y;
    const int raw_y = raw_height - 1 - x;
    DrawRawPixel(framebuffer, raw_width, raw_height, raw_x, raw_y, black);
}

bool ShouldDrawBlackForTone(int x, int y, uint8_t tone)
{
    if (tone <= design::color::kGray1) {
        return true;
    }
    if (tone <= design::color::kGray2) {
        return ((x + y) & 1) == 0;
    }
    if (tone < design::color::kGray4) {
        return (x % 2 == 0) && (y % 2 == 0);
    }
    return false;
}

void FillPortraitRect(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const UiRect& rect,
                      uint8_t tone)
{
    if (rect.IsEmpty()) {
        return;
    }

    for (int row = 0; row < rect.height; ++row) {
        for (int col = 0; col < rect.width; ++col) {
            const int px = rect.x + col;
            const int py = rect.y + row;
            DrawPortraitPixel(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              px,
                              py,
                              ShouldDrawBlackForTone(px, py, tone));
        }
    }
}

void DrawPortraitBorder(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        const UiRect& rect,
                        int thickness,
                        uint8_t tone)
{
    if (rect.IsEmpty() || thickness <= 0) {
        return;
    }

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {rect.x, rect.y, rect.width, std::min(thickness, rect.height)},
                     tone);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {rect.x,
                      rect.bottom() - std::min(thickness, rect.height),
                      rect.width,
                      std::min(thickness, rect.height)},
                     tone);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {rect.x, rect.y, std::min(thickness, rect.width), rect.height},
                     tone);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {rect.right() - std::min(thickness, rect.width),
                      rect.y,
                      std::min(thickness, rect.width),
                      rect.height},
                     tone);
}

void DrawTextLine(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int x,
                  int y,
                  std::string_view text,
                  design::TypographyRole role,
                  uint8_t tone)
{
    epaper_ui::DrawText(
        [&](int px, int py, uint8_t color) {
            DrawPortraitPixel(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              px,
                              py,
                              ShouldDrawBlackForTone(px, py, color));
        },
        x,
        y,
        text,
        tone,
        role);
}

std::array<std::string_view, 4> WrapLines(std::string_view text, int max_width, int* line_count)
{
    std::array<std::string_view, 4> lines = {};
    *line_count = 0;
    if (text.empty() || max_width <= 0) {
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

int CardHeight(int portrait_width)
{
    const int card_width = std::max(0, portrait_width - (2 * design::modal::kScreenInset));
    const int content_width =
        std::max(0, card_width - (2 * design::modal::kHorizontalPadding));
    int body_line_count = 0;
    (void)WrapLines(kBodyText, content_width, &body_line_count);

    const int title_height = LineHeight(kTitleRole);
    const int body_height = body_line_count * LineHeight(kBodyRole);
    return design::modal::kTopPadding + title_height + design::modal::kTitleContentGap +
           body_height + design::modal::kContentActionGap + design::button::kHeight +
           design::modal::kBottomPadding;
}

UiRect ActionBoundsFromCard(const UiRect& card, ShutdownModalAction action)
{
    const int inner_x = card.x + design::modal::kHorizontalPadding;
    const int inner_width = card.width - (2 * design::modal::kHorizontalPadding);
    const int gap = design::modal::kActionGap;
    const int button_width = std::max(0, (inner_width - gap) / 2);
    const int y = card.bottom() - design::modal::kBottomPadding - design::button::kHeight;
    const int x = action == ShutdownModalAction::kCancel ? inner_x : inner_x + button_width + gap;
    return {x, y, button_width, design::button::kHeight};
}

void DrawActionButton(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const UiRect& bounds,
                      std::string_view label,
                      bool selected)
{
    const uint8_t fill = selected ? design::color::kBlack : design::color::kWhite;
    const uint8_t border = design::color::kBlack;
    const uint8_t text = selected ? design::color::kWhite : design::color::kBlack;
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     bounds,
                     fill);
    DrawPortraitBorder(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       bounds,
                       design::button::kBorderThickness,
                       border);

    const int text_x = bounds.x + std::max(0, (bounds.width - MeasureText(kButtonRole, label)) / 2);
    const int text_y = bounds.y + std::max(0, (bounds.height - LineHeight(kButtonRole)) / 2);
    DrawTextLine(framebuffer,
                 raw_width,
                 raw_height,
                 portrait_width,
                 portrait_height,
                 text_x,
                 text_y,
                 label,
                 kButtonRole,
                 text);
}

}  // namespace

UiRect ShutdownModalCardBounds(int portrait_width, int portrait_height)
{
    const int card_width = std::max(0, portrait_width - (2 * design::modal::kScreenInset));
    const int card_height = CardHeight(portrait_width);
    return {(portrait_width - card_width) / 2,
            std::max(0, (portrait_height - card_height) / 2),
            card_width,
            card_height};
}

UiRect ShutdownModalActionBounds(int portrait_width,
                                 int portrait_height,
                                 ShutdownModalAction action)
{
    return ActionBoundsFromCard(ShutdownModalCardBounds(portrait_width, portrait_height), action);
}

ShutdownModalAction HitTestShutdownModalAction(int portrait_width,
                                               int portrait_height,
                                               int x,
                                               int y,
                                               bool* hit)
{
    const UiRect cancel_bounds =
        ShutdownModalActionBounds(portrait_width, portrait_height, ShutdownModalAction::kCancel);
    if (cancel_bounds.Contains(x, y)) {
        if (hit != nullptr) {
            *hit = true;
        }
        return ShutdownModalAction::kCancel;
    }

    const UiRect confirm_bounds =
        ShutdownModalActionBounds(portrait_width, portrait_height, ShutdownModalAction::kConfirm);
    if (confirm_bounds.Contains(x, y)) {
        if (hit != nullptr) {
            *hit = true;
        }
        return ShutdownModalAction::kConfirm;
    }

    if (hit != nullptr) {
        *hit = false;
    }
    return ShutdownModalAction::kCancel;
}

void DrawShutdownModal(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const ShutdownModalState& state)
{
    if (!state.visible || framebuffer == nullptr) {
        return;
    }

    const UiRect card = ShutdownModalCardBounds(portrait_width, portrait_height);
    const UiRect shadow = {card.x + design::modal::kShadowOffset,
                           card.y + design::modal::kShadowOffset,
                           card.width,
                           card.height};
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
    const int content_width =
        std::max(0, card.width - (2 * design::modal::kHorizontalPadding));
    const int title_y = card.y + design::modal::kTopPadding;
    DrawTextLine(framebuffer,
                 raw_width,
                 raw_height,
                 portrait_width,
                 portrait_height,
                 inner_x,
                 title_y,
                 kTitleText,
                 kTitleRole,
                 design::color::kBlack);

    int line_count = 0;
    const auto lines = WrapLines(kBodyText, content_width, &line_count);
    int body_y = title_y + LineHeight(kTitleRole) + design::modal::kTitleContentGap;
    for (int index = 0; index < line_count; ++index) {
        DrawTextLine(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     inner_x,
                     body_y,
                     lines[static_cast<size_t>(index)],
                     kBodyRole,
                     design::color::kBlack);
        body_y += LineHeight(kBodyRole);
    }

    DrawActionButton(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     ActionBoundsFromCard(card, ShutdownModalAction::kCancel),
                     kCancelLabel,
                     state.selected_action == ShutdownModalAction::kCancel);
    DrawActionButton(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     ActionBoundsFromCard(card, ShutdownModalAction::kConfirm),
                     kConfirmLabel,
                     state.selected_action == ShutdownModalAction::kConfirm);
}

}  // namespace epaper_ui
