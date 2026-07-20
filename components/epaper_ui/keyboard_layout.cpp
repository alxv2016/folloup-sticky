#include "epaper_ui/keyboard_layout.h"

#include <cstddef>

namespace epaper_ui {
namespace {

constexpr KeyboardKeySpec kLettersLowerRow1[] = {
    {.label = "q", .output = 'q'}, {.label = "w", .output = 'w'},
    {.label = "e", .output = 'e'}, {.label = "r", .output = 'r'},
    {.label = "t", .output = 't'}, {.label = "y", .output = 'y'},
    {.label = "u", .output = 'u'}, {.label = "i", .output = 'i'},
    {.label = "o", .output = 'o'}, {.label = "p", .output = 'p'},
};
constexpr KeyboardKeySpec kLettersLowerRow2[] = {
    {.label = "a", .output = 'a'}, {.label = "s", .output = 's'},
    {.label = "d", .output = 'd'}, {.label = "f", .output = 'f'},
    {.label = "g", .output = 'g'}, {.label = "h", .output = 'h'},
    {.label = "j", .output = 'j'}, {.label = "k", .output = 'k'},
    {.label = "l", .output = 'l'},
};
constexpr KeyboardKeySpec kLettersLowerRow3[] = {
    {.kind = KeyboardKeyKind::kShift, .label = "Shift", .width_units = 3},
    {.label = "z", .output = 'z'}, {.label = "x", .output = 'x'},
    {.label = "c", .output = 'c'}, {.label = "v", .output = 'v'},
    {.label = "b", .output = 'b'}, {.label = "n", .output = 'n'},
    {.label = "m", .output = 'm'},
    {.kind = KeyboardKeyKind::kBackspace, .label = "Bksp", .width_units = 3},
};
constexpr KeyboardKeySpec kLettersLowerRow4[] = {
    {.kind = KeyboardKeyKind::kMode123, .label = "123", .width_units = 5},
    {.kind = KeyboardKeyKind::kSpace, .label = "Space", .width_units = 10},
    {.kind = KeyboardKeyKind::kEnter, .label = "Done", .width_units = 5},
};
constexpr KeyboardRowSpec kLettersLowerRows[] = {
    {kLettersLowerRow1, std::size(kLettersLowerRow1)},
    {kLettersLowerRow2, std::size(kLettersLowerRow2)},
    {kLettersLowerRow3, std::size(kLettersLowerRow3)},
    {kLettersLowerRow4, std::size(kLettersLowerRow4)},
};

constexpr KeyboardKeySpec kLettersUpperRow1[] = {
    {.label = "Q", .output = 'Q'}, {.label = "W", .output = 'W'},
    {.label = "E", .output = 'E'}, {.label = "R", .output = 'R'},
    {.label = "T", .output = 'T'}, {.label = "Y", .output = 'Y'},
    {.label = "U", .output = 'U'}, {.label = "I", .output = 'I'},
    {.label = "O", .output = 'O'}, {.label = "P", .output = 'P'},
};
constexpr KeyboardKeySpec kLettersUpperRow2[] = {
    {.label = "A", .output = 'A'}, {.label = "S", .output = 'S'},
    {.label = "D", .output = 'D'}, {.label = "F", .output = 'F'},
    {.label = "G", .output = 'G'}, {.label = "H", .output = 'H'},
    {.label = "J", .output = 'J'}, {.label = "K", .output = 'K'},
    {.label = "L", .output = 'L'},
};
constexpr KeyboardKeySpec kLettersUpperRow3[] = {
    {.kind = KeyboardKeyKind::kShift, .label = "Shift", .width_units = 3},
    {.label = "Z", .output = 'Z'}, {.label = "X", .output = 'X'},
    {.label = "C", .output = 'C'}, {.label = "V", .output = 'V'},
    {.label = "B", .output = 'B'}, {.label = "N", .output = 'N'},
    {.label = "M", .output = 'M'},
    {.kind = KeyboardKeyKind::kBackspace, .label = "Bksp", .width_units = 3},
};
constexpr KeyboardKeySpec kLettersUpperRow4[] = {
    {.kind = KeyboardKeyKind::kMode123, .label = "123", .width_units = 5},
    {.kind = KeyboardKeyKind::kSpace, .label = "Space", .width_units = 10},
    {.kind = KeyboardKeyKind::kEnter, .label = "Done", .width_units = 5},
};
constexpr KeyboardRowSpec kLettersUpperRows[] = {
    {kLettersUpperRow1, std::size(kLettersUpperRow1)},
    {kLettersUpperRow2, std::size(kLettersUpperRow2)},
    {kLettersUpperRow3, std::size(kLettersUpperRow3)},
    {kLettersUpperRow4, std::size(kLettersUpperRow4)},
};

constexpr KeyboardKeySpec kSymbolsRow1[] = {
    {.label = "1", .output = '1'}, {.label = "2", .output = '2'},
    {.label = "3", .output = '3'}, {.label = "4", .output = '4'},
    {.label = "5", .output = '5'}, {.label = "6", .output = '6'},
    {.label = "7", .output = '7'}, {.label = "8", .output = '8'},
    {.label = "9", .output = '9'}, {.label = "0", .output = '0'},
};
constexpr KeyboardKeySpec kSymbolsRow2[] = {
    {.label = "@", .output = '@'}, {.label = "#", .output = '#'},
    {.label = "$", .output = '$'}, {.label = "%", .output = '%'},
    {.label = "&", .output = '&'}, {.label = "-", .output = '-'},
    {.label = "+", .output = '+'}, {.label = "(", .output = '('},
    {.label = ")", .output = ')'}, {.label = "/", .output = '/'},
};
constexpr KeyboardKeySpec kSymbolsRow3[] = {
    {.kind = KeyboardKeyKind::kModeMore, .label = "#+=", .width_units = 3},
    {.label = ".", .output = '.'}, {.label = ",", .output = ','},
    {.label = "?", .output = '?'}, {.label = "!", .output = '!'},
    {.label = "'", .output = '\''}, {.label = "\"", .output = '"'},
    {.label = ":", .output = ':'},
    {.kind = KeyboardKeyKind::kBackspace, .label = "Bksp", .width_units = 3},
};
constexpr KeyboardKeySpec kSymbolsRow4[] = {
    {.kind = KeyboardKeyKind::kModeAbc, .label = "ABC", .width_units = 5},
    {.kind = KeyboardKeyKind::kSpace, .label = "Space", .width_units = 10},
    {.kind = KeyboardKeyKind::kEnter, .label = "Done", .width_units = 5},
};
constexpr KeyboardRowSpec kSymbolsRows[] = {
    {kSymbolsRow1, std::size(kSymbolsRow1)},
    {kSymbolsRow2, std::size(kSymbolsRow2)},
    {kSymbolsRow3, std::size(kSymbolsRow3)},
    {kSymbolsRow4, std::size(kSymbolsRow4)},
};

// Second symbols page (reached from the "#+=" key on the symbols page).
constexpr KeyboardKeySpec kSymbols2Row1[] = {
    {.label = "[", .output = '['}, {.label = "]", .output = ']'},
    {.label = "{", .output = '{'}, {.label = "}", .output = '}'},
    {.label = "#", .output = '#'}, {.label = "%", .output = '%'},
    {.label = "^", .output = '^'}, {.label = "*", .output = '*'},
    {.label = "+", .output = '+'}, {.label = "=", .output = '='},
};
constexpr KeyboardKeySpec kSymbols2Row2[] = {
    {.label = "_", .output = '_'}, {.label = "\\", .output = '\\'},
    {.label = "|", .output = '|'}, {.label = "~", .output = '~'},
    {.label = "<", .output = '<'}, {.label = ">", .output = '>'},
    {.label = ";", .output = ';'}, {.label = ":", .output = ':'},
    {.label = "`", .output = '`'}, {.label = "/", .output = '/'},
};
constexpr KeyboardKeySpec kSymbols2Row3[] = {
    {.kind = KeyboardKeyKind::kMode123, .label = "123", .width_units = 3},
    {.label = ".", .output = '.'}, {.label = ",", .output = ','},
    {.label = "?", .output = '?'}, {.label = "!", .output = '!'},
    {.label = "'", .output = '\''}, {.label = "\"", .output = '"'},
    {.label = "@", .output = '@'},
    {.kind = KeyboardKeyKind::kBackspace, .label = "Bksp", .width_units = 3},
};
constexpr KeyboardKeySpec kSymbols2Row4[] = {
    {.kind = KeyboardKeyKind::kModeAbc, .label = "ABC", .width_units = 5},
    {.kind = KeyboardKeyKind::kSpace, .label = "Space", .width_units = 10},
    {.kind = KeyboardKeyKind::kEnter, .label = "Done", .width_units = 5},
};
constexpr KeyboardRowSpec kSymbols2Rows[] = {
    {kSymbols2Row1, std::size(kSymbols2Row1)},
    {kSymbols2Row2, std::size(kSymbols2Row2)},
    {kSymbols2Row3, std::size(kSymbols2Row3)},
    {kSymbols2Row4, std::size(kSymbols2Row4)},
};

constexpr KeyboardKeySpec kNumbersRow1[] = {
    {.label = "1", .output = '1', .width_units = 3},
    {.label = "2", .output = '2', .width_units = 3},
    {.label = "3", .output = '3', .width_units = 3},
};
constexpr KeyboardKeySpec kNumbersRow2[] = {
    {.label = "4", .output = '4', .width_units = 3},
    {.label = "5", .output = '5', .width_units = 3},
    {.label = "6", .output = '6', .width_units = 3},
};
constexpr KeyboardKeySpec kNumbersRow3[] = {
    {.label = "7", .output = '7', .width_units = 3},
    {.label = "8", .output = '8', .width_units = 3},
    {.label = "9", .output = '9', .width_units = 3},
};
constexpr KeyboardKeySpec kNumbersRow4[] = {
    {.kind = KeyboardKeyKind::kBackspace, .label = "Bksp", .width_units = 3},
    {.label = "0", .output = '0', .width_units = 3},
    {.kind = KeyboardKeyKind::kEnter, .label = "Done", .width_units = 3},
};
constexpr KeyboardRowSpec kNumbersRows[] = {
    {kNumbersRow1, std::size(kNumbersRow1)},
    {kNumbersRow2, std::size(kNumbersRow2)},
    {kNumbersRow3, std::size(kNumbersRow3)},
    {kNumbersRow4, std::size(kNumbersRow4)},
};

constexpr KeyboardLayoutSpec kLettersLowerLayout = {kLettersLowerRows,
                                                    std::size(kLettersLowerRows)};
constexpr KeyboardLayoutSpec kLettersUpperLayout = {kLettersUpperRows,
                                                    std::size(kLettersUpperRows)};
constexpr KeyboardLayoutSpec kSymbolsLayout = {kSymbolsRows, std::size(kSymbolsRows)};
constexpr KeyboardLayoutSpec kSymbols2Layout = {kSymbols2Rows, std::size(kSymbols2Rows)};
constexpr KeyboardLayoutSpec kNumbersLayout = {kNumbersRows, std::size(kNumbersRows)};

}  // namespace

const KeyboardLayoutSpec& GetKeyboardLayout(KeyboardLayoutKind kind)
{
    switch (kind) {
        case KeyboardLayoutKind::kLettersLower:
            return kLettersLowerLayout;
        case KeyboardLayoutKind::kLettersUpper:
            return kLettersUpperLayout;
        case KeyboardLayoutKind::kSymbols:
            return kSymbolsLayout;
        case KeyboardLayoutKind::kSymbols2:
            return kSymbols2Layout;
        case KeyboardLayoutKind::kNumbers:
            return kNumbersLayout;
    }
    return kLettersLowerLayout;
}

size_t KeyboardKeyCount(KeyboardLayoutKind kind)
{
    const KeyboardLayoutSpec& layout = GetKeyboardLayout(kind);
    size_t count = 0;
    for (size_t row = 0; row < layout.row_count; ++row) {
        count += layout.rows[row].key_count;
    }
    return count;
}

const KeyboardKeySpec* KeyboardKeyAt(KeyboardLayoutKind kind,
                                     size_t flat_index,
                                     int* out_row,
                                     int* out_column)
{
    const KeyboardLayoutSpec& layout = GetKeyboardLayout(kind);
    size_t current = 0;
    for (size_t row = 0; row < layout.row_count; ++row) {
        const KeyboardRowSpec& row_spec = layout.rows[row];
        if (flat_index < current + row_spec.key_count) {
            const int column = static_cast<int>(flat_index - current);
            if (out_row != nullptr) {
                *out_row = static_cast<int>(row);
            }
            if (out_column != nullptr) {
                *out_column = column;
            }
            return &row_spec.keys[column];
        }
        current += row_spec.key_count;
    }
    return nullptr;
}

int KeyboardFlatIndex(KeyboardLayoutKind kind, int row, int column)
{
    const KeyboardLayoutSpec& layout = GetKeyboardLayout(kind);
    if (row < 0 || static_cast<size_t>(row) >= layout.row_count) {
        return -1;
    }

    int flat_index = 0;
    for (int row_index = 0; row_index < row; ++row_index) {
        flat_index += static_cast<int>(layout.rows[row_index].key_count);
    }
    if (column < 0 || static_cast<size_t>(column) >= layout.rows[row].key_count) {
        return -1;
    }
    return flat_index + column;
}

std::string_view KeyboardSubmitLabel(KeyboardInputSubmitStyle style)
{
    switch (style) {
        case KeyboardInputSubmitStyle::kDone:
            return "Done";
        case KeyboardInputSubmitStyle::kJoin:
            return "Join";
        case KeyboardInputSubmitStyle::kSave:
            return "Save";
        case KeyboardInputSubmitStyle::kNext:
            return "Next";
    }
    return "Done";
}

}  // namespace epaper_ui
