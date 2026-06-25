#ifndef EPAPER_UI_KEYBOARD_H_
#define EPAPER_UI_KEYBOARD_H_

#include <cstdint>
#include <string>

#include "epaper_ui/button.h"
#include "epaper_ui/keyboard_input.h"
#include "epaper_ui/keyboard_layout.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

struct KeyboardState {
    bool visible = false;
    std::string title_text = {};
    KeyboardInputState input = {};
    KeyboardLayoutKind layout = KeyboardLayoutKind::kLettersLower;
    int selected_key_index = 0;
    bool shift_locked = false;

    bool operator==(const KeyboardState& other) const = default;
};

struct KeyboardStyle {
    int screen_origin_y = 0;
    int screen_height = 0;
    int vertical_padding = design::spacing::k16;
    int horizontal_padding = design::spacing::k14;
    int row_gap = design::spacing::k2;
    int key_gap = design::spacing::k2;
    int field_gap = design::spacing::k16;
    int title_gap = design::spacing::k8;
    int border_thickness = 2;
    int shadow_offset = design::spacing::k8;
    int key_height = 44;
    uint8_t sheet_color = design::color::kWhite;
    uint8_t shadow_color = design::color::kGrayMid;
    uint8_t border_color = design::color::kBlack;
    design::TypographyRole title_role = design::TypographyRole::kHeadingH2;
    ButtonStyle key_button = {
        .role = design::TypographyRole::kLabelSmall,
        .background_color = design::color::kGrayLight,
        .selected_background_color = design::color::kBlack,
        .border_color = design::color::kBlack,
        .text_color = design::color::kBlack,
        .selected_text_color = design::color::kWhite,
        .width = 0,
        .min_width = 0,
        .height = 44,
        .horizontal_padding = design::spacing::k8,
        .center_label = true,
        .border_thickness = 2,
        .stroke_thickness = 2,
    };
    ButtonStyle special_key_button = {
        .role = design::TypographyRole::kLabelSmall,
        .background_color = design::color::kGrayMid,
        .selected_background_color = design::color::kBlack,
        .border_color = design::color::kBlack,
        .text_color = design::color::kBlack,
        .selected_text_color = design::color::kWhite,
        .width = 0,
        .min_width = 0,
        .height = 44,
        .horizontal_padding = design::spacing::k8,
        .center_label = true,
        .border_thickness = 2,
        .stroke_thickness = 2,
    };
};

UiRect KeyboardPanelBounds(int portrait_width,
                           int portrait_height,
                           const KeyboardState& state,
                           const KeyboardStyle& style);
UiRect KeyboardInputBounds(int portrait_width,
                           int portrait_height,
                           const KeyboardState& state,
                           const KeyboardStyle& style);
UiRect KeyboardKeyBounds(int portrait_width,
                         int portrait_height,
                         const KeyboardState& state,
                         const KeyboardStyle& style,
                         int row,
                         int column);
bool HitTestKeyboardKey(int portrait_width,
                        int portrait_height,
                        const KeyboardState& state,
                        const KeyboardStyle& style,
                        int x,
                        int y,
                        int* flat_key_index);
void DrawKeyboard(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  const KeyboardState& state,
                  const KeyboardStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_KEYBOARD_H_
