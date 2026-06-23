#include "epaper_ui/keyboard_controller.h"

#include <algorithm>

namespace epaper_ui {
namespace {

int WrapIndex(int index, int count)
{
    if (count <= 0) {
        return 0;
    }
    int wrapped = index % count;
    if (wrapped < 0) {
        wrapped += count;
    }
    return wrapped;
}

void NormalizeCursor(KeyboardInputState& input)
{
    input.cursor_index =
        std::clamp(input.cursor_index < 0 ? static_cast<int>(input.value_text.size())
                                          : input.cursor_index,
                   0,
                   static_cast<int>(input.value_text.size()));
}

bool InsertText(KeyboardInputState& input, char character)
{
    if (character == '\0' || input.value_text.size() >= input.max_length) {
        return false;
    }

    NormalizeCursor(input);
    input.value_text.insert(input.value_text.begin() + input.cursor_index, character);
    ++input.cursor_index;
    return true;
}

bool Backspace(KeyboardInputState& input)
{
    NormalizeCursor(input);
    if (input.cursor_index <= 0 || input.value_text.empty()) {
        return false;
    }

    input.value_text.erase(static_cast<size_t>(input.cursor_index - 1), 1);
    --input.cursor_index;
    return true;
}

}  // namespace

bool KeyboardController::MoveFocus(KeyboardState& state, int delta)
{
    const int key_count = static_cast<int>(KeyboardKeyCount(state.layout));
    if (key_count <= 0) {
        state.selected_key_index = 0;
        return false;
    }

    const int next = WrapIndex(state.selected_key_index + delta, key_count);
    if (next == state.selected_key_index) {
        return false;
    }
    state.selected_key_index = next;
    return true;
}

KeyboardActionResult KeyboardController::ActivateFocusedKey(KeyboardState& state, bool long_press)
{
    KeyboardActionResult result = {};
    ClampSelection(state);

    const KeyboardKeySpec* key =
        KeyboardKeyAt(state.layout, static_cast<size_t>(state.selected_key_index));
    if (key == nullptr) {
        return result;
    }

    switch (key->kind) {
        case KeyboardKeyKind::kCharacter:
            result.text_changed = InsertText(state.input, key->output);
            result.state_changed = result.text_changed;
            if (state.layout == KeyboardLayoutKind::kLettersUpper && !state.shift_locked) {
                state.layout = KeyboardLayoutKind::kLettersLower;
                ClampSelection(state);
                result.state_changed = true;
            }
            return result;
        case KeyboardKeyKind::kShift:
            if (long_press) {
                const bool changed =
                    !state.shift_locked || state.layout != KeyboardLayoutKind::kLettersUpper;
                state.shift_locked = true;
                state.layout = KeyboardLayoutKind::kLettersUpper;
                result.state_changed = changed;
                return result;
            }
            if (state.shift_locked) {
                state.shift_locked = false;
                state.layout = KeyboardLayoutKind::kLettersLower;
                result.state_changed = true;
                return result;
            }
            state.layout = state.layout == KeyboardLayoutKind::kLettersUpper
                               ? KeyboardLayoutKind::kLettersLower
                               : KeyboardLayoutKind::kLettersUpper;
            ClampSelection(state);
            result.state_changed = true;
            return result;
        case KeyboardKeyKind::kBackspace:
            result.text_changed = Backspace(state.input);
            result.state_changed = result.text_changed;
            return result;
        case KeyboardKeyKind::kSpace:
            result.text_changed = InsertText(state.input, ' ');
            result.state_changed = result.text_changed;
            return result;
        case KeyboardKeyKind::kEnter:
            result.intent = KeyboardIntent::kSubmit;
            return result;
        case KeyboardKeyKind::kMode123:
            state.shift_locked = false;
            state.layout = KeyboardLayoutKind::kSymbols;
            ClampSelection(state);
            result.state_changed = true;
            return result;
        case KeyboardKeyKind::kModeAbc:
            state.shift_locked = false;
            state.layout = KeyboardLayoutKind::kLettersLower;
            ClampSelection(state);
            result.state_changed = true;
            return result;
        case KeyboardKeyKind::kDismiss:
            result.intent = KeyboardIntent::kDismiss;
            return result;
    }

    return result;
}

KeyboardActionResult KeyboardController::ActivateFocusedKeyDouble(KeyboardState& state)
{
    KeyboardActionResult result = {};
    ClampSelection(state);

    const KeyboardKeySpec* key =
        KeyboardKeyAt(state.layout, static_cast<size_t>(state.selected_key_index));
    if (key == nullptr) {
        return result;
    }

    if (key->kind == KeyboardKeyKind::kShift) {
        const bool changed =
            !state.shift_locked || state.layout != KeyboardLayoutKind::kLettersUpper;
        state.shift_locked = true;
        state.layout = KeyboardLayoutKind::kLettersUpper;
        ClampSelection(state);
        result.state_changed = changed;
        return result;
    }

    result.intent = KeyboardIntent::kDismiss;
    return result;
}

void KeyboardController::ClampSelection(KeyboardState& state)
{
    const int key_count = static_cast<int>(KeyboardKeyCount(state.layout));
    if (key_count <= 0) {
        state.selected_key_index = 0;
        return;
    }
    state.selected_key_index = WrapIndex(state.selected_key_index, key_count);
    NormalizeCursor(state.input);
}

}  // namespace epaper_ui
