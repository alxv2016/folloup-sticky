#ifndef EPAPER_UI_KEYBOARD_LAYOUT_H_
#define EPAPER_UI_KEYBOARD_LAYOUT_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "epaper_ui/keyboard_input.h"

namespace epaper_ui {

enum class KeyboardLayoutKind : uint8_t {
    kLettersLower = 0,
    kLettersUpper,
    kSymbols,
    kSymbols2,
    kNumbers,
};

enum class KeyboardKeyKind : uint8_t {
    kCharacter = 0,
    kShift,
    kBackspace,
    kSpace,
    kEnter,
    kMode123,
    kModeAbc,
    kModeMore,
    kDismiss,
};

struct KeyboardKeySpec {
    KeyboardKeyKind kind = KeyboardKeyKind::kCharacter;
    std::string_view label = {};
    char output = '\0';
    // Width in grid columns. The grid uses half-letter granularity: a single character key
    // is 2 columns, so wider control keys can be 1.5x a letter (3 columns) without
    // overflowing the panel. Letters end up square (44x44) on the 480px panel.
    int width_units = 2;
};

struct KeyboardRowSpec {
    const KeyboardKeySpec* keys = nullptr;
    size_t key_count = 0;
};

struct KeyboardLayoutSpec {
    const KeyboardRowSpec* rows = nullptr;
    size_t row_count = 0;
};

const KeyboardLayoutSpec& GetKeyboardLayout(KeyboardLayoutKind kind);
size_t KeyboardKeyCount(KeyboardLayoutKind kind);
const KeyboardKeySpec* KeyboardKeyAt(KeyboardLayoutKind kind,
                                     size_t flat_index,
                                     int* out_row = nullptr,
                                     int* out_column = nullptr);
int KeyboardFlatIndex(KeyboardLayoutKind kind, int row, int column);
std::string_view KeyboardSubmitLabel(KeyboardInputSubmitStyle style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_KEYBOARD_LAYOUT_H_
