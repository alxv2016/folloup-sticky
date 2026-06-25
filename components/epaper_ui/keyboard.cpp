#include "epaper_ui/keyboard.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kControlCornerRadius = 4;

int TotalWidthUnits(const KeyboardRowSpec& row)
{
    int total = 0;
    for (size_t index = 0; index < row.key_count; ++index) {
        total += std::max(1, row.keys[index].width_units);
    }
    return total;
}

int WidthUnitsBefore(const KeyboardRowSpec& row, int column)
{
    int total = 0;
    for (int index = 0; index < column; ++index) {
        total += std::max(1, row.keys[index].width_units);
    }
    return total;
}

int RowsBlockHeight(const KeyboardState& state, const KeyboardStyle& style)
{
    const KeyboardLayoutSpec& layout = GetKeyboardLayout(state.layout);
    if (layout.row_count == 0) {
        return 0;
    }
    return static_cast<int>(layout.row_count) * ClampPositive(style.key_height) +
           std::max<int>(0, static_cast<int>(layout.row_count) - 1) *
               ClampPositive(style.row_gap);
}

std::string BuildDisplayText(const KeyboardInputState& input)
{
    // The keyboard modal always shows the typed text in full, even for password fields,
    // so the user can verify what they entered while the on-screen keyboard is open.
    // (The page-level password field keeps its own masking/reveal behavior; password_mode
    // still round-trips through ApplyKeyboardInputToPasswordInput.)
    std::string display = input.value_text;
    if (!input.show_cursor) {
        return display;
    }
    const int cursor = std::clamp(input.cursor_index < 0 ? static_cast<int>(display.size())
                                                         : input.cursor_index,
                                  0,
                                  static_cast<int>(display.size()));
    display.insert(display.begin() + cursor, '|');
    return display;
}

std::string FitText(std::string_view text, design::TypographyRole role, int max_width)
{
    if (text.empty() || max_width <= 0) {
        return {};
    }
    if (MeasureText(role, text) <= max_width) {
        return std::string(text);
    }

    constexpr std::string_view kEllipsis = "...";
    if (MeasureText(role, kEllipsis) > max_width) {
        return {};
    }

    size_t length = text.size();
    while (length > 0) {
        std::string candidate = std::string(text.substr(0, length)) + std::string(kEllipsis);
        if (MeasureText(role, candidate) <= max_width) {
            return candidate;
        }
        --length;
    }
    return std::string(kEllipsis);
}

UiRect Inset(const UiRect& rect, int inset)
{
    return {rect.x + inset,
            rect.y + inset,
            std::max(0, rect.width - (2 * inset)),
            std::max(0, rect.height - (2 * inset))};
}

void DrawKeyboardInputField(uint8_t* framebuffer,
                            int raw_width,
                            int raw_height,
                            int portrait_width,
                            int portrait_height,
                            const UiRect& bounds,
                            const KeyboardState& state,
                            const KeyboardStyle& style)
{
    if (!state.input.label_text.empty()) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           bounds.x,
                           bounds.y,
                           state.input.label_text,
                           design::TypographyRole::kLabelMedium,
                           design::color::kBlack);
    }

    const int label_height =
        state.input.label_text.empty() ? 0 : LineHeight(design::TypographyRole::kLabelMedium);
    const int label_gap = state.input.label_text.empty() ? 0 : design::password_input::kLabelGap;
    const UiRect field = {bounds.x,
                          bounds.y + label_height + label_gap,
                          bounds.width,
                          design::password_input::kFieldHeight};
    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            field,
                            kControlCornerRadius,
                            design::color::kWhite);
    DrawRoundedPortraitBorder(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              field,
                              kControlCornerRadius,
                              design::password_input::kBorderThickness,
                              design::color::kBlack);

    const UiRect inner = Inset(field, design::password_input::kBorderThickness);
    const bool show_placeholder =
        state.input.value_text.empty() && !state.input.placeholder_text.empty();
    const std::string raw_text =
        show_placeholder ? state.input.placeholder_text : BuildDisplayText(state.input);
    const design::TypographyRole role = design::TypographyRole::kInputValue;
    const uint8_t tone = show_placeholder ? design::color::kGrayDark : design::color::kBlack;
    const std::string text = FitText(raw_text,
                                     role,
                                     std::max(0,
                                              inner.width - (2 * design::password_input::kHorizontalPadding)));
    if (text.empty()) {
        return;
    }

    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       inner.x + design::password_input::kHorizontalPadding,
                       inner.y + CenterOffset(inner.height, LineHeight(role)),
                       text,
                       role,
                       tone);
}

}  // namespace

UiRect KeyboardPanelBounds(int portrait_width,
                           int portrait_height,
                           const KeyboardState& state,
                           const KeyboardStyle& style)
{
    if (!state.visible) {
        return {};
    }

    const int vertical_padding = ClampPositive(style.vertical_padding);
    const int title_height = state.title_text.empty() ? 0 : LineHeight(style.title_role);
    const int title_gap = title_height > 0 ? ClampPositive(style.title_gap) : 0;
    const int input_height =
        design::password_input::kFieldHeight +
        (state.input.label_text.empty() ? 0
                                        : LineHeight(design::TypographyRole::kLabelMedium) +
                                              design::password_input::kLabelGap);
    const int field_gap = input_height > 0 ? ClampPositive(style.field_gap) : 0;
    const int height = (2 * vertical_padding) + title_height + title_gap + input_height +
                       field_gap + RowsBlockHeight(state, style);
    const int y = style.screen_origin_y > 0
                      ? style.screen_origin_y
                      : std::max(0, portrait_height - std::min(portrait_height, height));
    return {0, y, portrait_width, std::min(portrait_height, height)};
}

UiRect KeyboardInputBounds(int portrait_width,
                           int portrait_height,
                           const KeyboardState& state,
                           const KeyboardStyle& style)
{
    const UiRect panel = KeyboardPanelBounds(portrait_width, portrait_height, state, style);
    if (panel.IsEmpty()) {
        return {};
    }

    const int x = panel.x + ClampPositive(style.horizontal_padding);
    const int width = std::max(0, panel.width - (2 * ClampPositive(style.horizontal_padding)));
    int y = panel.y + ClampPositive(style.vertical_padding);
    if (!state.title_text.empty()) {
        y += LineHeight(style.title_role) + ClampPositive(style.title_gap);
    }
    const int label_height =
        state.input.label_text.empty() ? 0 : LineHeight(design::TypographyRole::kLabelMedium);
    return {x,
            y,
            width,
            design::password_input::kFieldHeight +
                (label_height > 0 ? label_height + design::password_input::kLabelGap : 0)};
}

UiRect KeyboardKeyBounds(int portrait_width,
                         int portrait_height,
                         const KeyboardState& state,
                         const KeyboardStyle& style,
                         int row,
                         int column)
{
    const KeyboardLayoutSpec& layout = GetKeyboardLayout(state.layout);
    if (row < 0 || static_cast<size_t>(row) >= layout.row_count) {
        return {};
    }
    const KeyboardRowSpec& row_spec = layout.rows[row];
    if (column < 0 || static_cast<size_t>(column) >= row_spec.key_count) {
        return {};
    }

    const UiRect panel = KeyboardPanelBounds(portrait_width, portrait_height, state, style);
    if (panel.IsEmpty()) {
        return {};
    }

    const UiRect input = KeyboardInputBounds(portrait_width, portrait_height, state, style);
    const int title_height = state.title_text.empty() ? 0 : LineHeight(style.title_role);
    const int title_gap = title_height > 0 ? ClampPositive(style.title_gap) : 0;
    const int field_gap = input.height > 0 ? ClampPositive(style.field_gap) : 0;
    const int content_x = panel.x + ClampPositive(style.horizontal_padding);
    const int content_width =
        std::max(0, panel.width - (2 * ClampPositive(style.horizontal_padding)));
    const int rows_y = panel.y + ClampPositive(style.vertical_padding) + title_height + title_gap +
                       input.height + field_gap;
    const int y = rows_y + row * (ClampPositive(style.key_height) + ClampPositive(style.row_gap));

    // Fixed grid: every row is laid out on the same column scale, derived from the widest
    // row in the layout, so a 1-unit key (every letter) is identical in every row instead
    // of being stretched to fill its own row. Edges are distributed proportionally (not a
    // floored integer column width) so the configured horizontal padding is honored
    // exactly and no width is wasted to rounding; rows narrower than the widest are
    // centered.
    int reference_units = 1;
    for (size_t r = 0; r < layout.row_count; ++r) {
        reference_units = std::max(reference_units, TotalWidthUnits(layout.rows[r]));
    }
    const int key_gap = ClampPositive(style.key_gap);
    const int row_units = TotalWidthUnits(row_spec);
    const int row_px = (row_units * content_width) / reference_units;
    const int row_x = content_x + std::max(0, (content_width - row_px) / 2);
    const int start_units = WidthUnitsBefore(row_spec, column);
    const int key_units = std::max(1, row_spec.keys[column].width_units);
    const int key_x0 = row_units > 0 ? (start_units * row_px) / row_units : 0;
    const int key_x1 = row_units > 0 ? ((start_units + key_units) * row_px) / row_units : row_px;
    const int x = row_x + key_x0;
    const int width = std::max(1, key_x1 - key_x0 - key_gap);
    return {x, y, width, ClampPositive(style.key_height)};
}

bool HitTestKeyboardKey(int portrait_width,
                        int portrait_height,
                        const KeyboardState& state,
                        const KeyboardStyle& style,
                        int x,
                        int y,
                        int* flat_key_index)
{
    if (flat_key_index != nullptr) {
        *flat_key_index = -1;
    }

    const UiRect panel = KeyboardPanelBounds(portrait_width, portrait_height, state, style);
    if (panel.IsEmpty()) {
        return false;
    }

    const KeyboardLayoutSpec& layout = GetKeyboardLayout(state.layout);
    int nearest_index = -1;
    long nearest_dist = -1;
    int rows_top = -1;
    for (size_t row = 0; row < layout.row_count; ++row) {
        for (size_t column = 0; column < layout.rows[row].key_count; ++column) {
            const UiRect bounds = KeyboardKeyBounds(portrait_width,
                                                    portrait_height,
                                                    state,
                                                    style,
                                                    static_cast<int>(row),
                                                    static_cast<int>(column));
            if (bounds.IsEmpty()) {
                continue;
            }
            if (rows_top < 0 || bounds.y < rows_top) {
                rows_top = bounds.y;
            }
            const int flat_index =
                KeyboardFlatIndex(state.layout, static_cast<int>(row), static_cast<int>(column));
            if (bounds.Contains(x, y)) {
                if (flat_key_index != nullptr) {
                    *flat_key_index = flat_index;
                }
                return true;
            }
            // Squared distance from the tap to the key rectangle (0 if inside, handled
            // above), for the nearest-key snap below.
            const int clamped_x = std::clamp(x, bounds.x, bounds.x + bounds.width);
            const int clamped_y = std::clamp(y, bounds.y, bounds.y + bounds.height);
            const long dx = x - clamped_x;
            const long dy = y - clamped_y;
            const long dist = dx * dx + dy * dy;
            if (nearest_dist < 0 || dist < nearest_dist) {
                nearest_dist = dist;
                nearest_index = flat_index;
            }
        }
    }

    // No key rectangle contained the tap. Snap to the nearest key, but only when the tap
    // falls within the rows region of the panel (first row top .. panel bottom, across the
    // panel width). This removes the dead zones in the inter-key gaps and the panel's
    // bottom/side padding: the bottom row in particular has no row beneath it, so a
    // slightly-low tap on the number/Hide/space/Join keys would otherwise miss entirely
    // and feel unresponsive. Taps above the first row (title/input field area) still fall
    // through so the input field keeps its own handling.
    if (nearest_index >= 0 && rows_top >= 0 && y >= rows_top && y <= panel.y + panel.height &&
        x >= panel.x && x <= panel.x + panel.width) {
        if (flat_key_index != nullptr) {
            *flat_key_index = nearest_index;
        }
        return true;
    }
    return false;
}

void DrawKeyboard(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  const KeyboardState& state,
                  const KeyboardStyle& style)
{
    const UiRect panel = KeyboardPanelBounds(portrait_width, portrait_height, state, style);
    if (panel.IsEmpty()) {
        return;
    }

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {panel.x, std::max(0, panel.y - ClampPositive(style.shadow_offset)), panel.width,
                      ClampPositive(style.shadow_offset)},
                     style.shadow_color);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     panel,
                     style.sheet_color);
    DrawPortraitBorder(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       panel,
                       style.border_thickness,
                       style.border_color);

    const int content_x = panel.x + ClampPositive(style.horizontal_padding);
    int content_y = panel.y + ClampPositive(style.vertical_padding);
    if (!state.title_text.empty()) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           content_x,
                           content_y,
                           state.title_text,
                           style.title_role,
                           design::color::kBlack);
        content_y += LineHeight(style.title_role) + ClampPositive(style.title_gap);
    }

    const UiRect input = KeyboardInputBounds(portrait_width, portrait_height, state, style);
    DrawKeyboardInputField(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           input,
                           state,
                           style);

    const KeyboardLayoutSpec& layout = GetKeyboardLayout(state.layout);
    for (size_t row = 0; row < layout.row_count; ++row) {
        for (size_t column = 0; column < layout.rows[row].key_count; ++column) {
            const int flat_index = KeyboardFlatIndex(
                state.layout, static_cast<int>(row), static_cast<int>(column));
            const KeyboardKeySpec& key = layout.rows[row].keys[column];
            const UiRect bounds = KeyboardKeyBounds(portrait_width,
                                                    portrait_height,
                                                    state,
                                                    style,
                                                    static_cast<int>(row),
                                                    static_cast<int>(column));
            ButtonStyle button_style =
                key.kind == KeyboardKeyKind::kCharacter ? style.key_button : style.special_key_button;
            button_style.width = bounds.width;
            button_style.height = bounds.height;
            button_style.border_thickness =
                button_style.border_thickness > 0 ? button_style.border_thickness : 2;
            button_style.stroke_thickness =
                button_style.stroke_thickness > 0 ? button_style.stroke_thickness : 2;
            button_style.center_label = true;
            DrawButton(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       bounds.x,
                       bounds.y,
                       {
                           .label_text = key.kind == KeyboardKeyKind::kEnter
                                             ? KeyboardSubmitLabel(state.input.submit_style)
                                             : key.label,
                           .selected = flat_index == state.selected_key_index,
                       },
                       button_style);
        }
    }
}

}  // namespace epaper_ui
