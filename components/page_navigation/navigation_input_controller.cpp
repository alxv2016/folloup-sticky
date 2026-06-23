#include "page_navigation/navigation_input_controller.h"

namespace page_navigation {

NavigationInputController::NavigationInputController(uint32_t hold_start_ms,
                                                     uint32_t hold_repeat_interval_ms)
    : hold_start_ms_(hold_start_ms), hold_repeat_interval_ms_(hold_repeat_interval_ms) {
    for (auto& pressed : pressed_) {
        pressed.store(false);
    }
    for (auto& generation : generation_) {
        generation.store(0);
    }
    last_hold_pressed_time_ms_.fill(0);
}

void NavigationInputController::BeginPress(size_t slot) {
    if (!IsValidSlot(slot)) {
        return;
    }
    generation_[slot].fetch_add(1);
    pressed_[slot].store(true);
    ResetHold(slot);
}

void NavigationInputController::EndPress(size_t slot) {
    if (!IsValidSlot(slot)) {
        return;
    }
    pressed_[slot].store(false);
    generation_[slot].fetch_add(1);
    ResetHold(slot);
}

void NavigationInputController::ResetHold(size_t slot) {
    if (!IsValidSlot(slot)) {
        return;
    }
    last_hold_pressed_time_ms_[slot] = 0;
}

uint32_t NavigationInputController::CurrentGeneration(size_t slot) const {
    if (!IsValidSlot(slot)) {
        return 0;
    }
    return generation_[slot].load();
}

bool NavigationInputController::IsCurrentPress(size_t slot, uint32_t generation) const {
    if (!IsValidSlot(slot)) {
        return false;
    }
    return pressed_[slot].load() && generation_[slot].load() == generation;
}

bool NavigationInputController::ShouldHandleHold(size_t slot, uint32_t generation,
                                                 uint32_t pressed_time_ms) {
    if (!IsCurrentPress(slot, generation) || pressed_time_ms < hold_start_ms_) {
        return false;
    }

    uint32_t& last_pressed_time_ms = last_hold_pressed_time_ms_[slot];
    if (pressed_time_ms < last_pressed_time_ms) {
        last_pressed_time_ms = 0;
    }

    if (last_pressed_time_ms != 0 &&
        pressed_time_ms - last_pressed_time_ms < hold_repeat_interval_ms_) {
        return false;
    }

    last_pressed_time_ms = pressed_time_ms;
    return true;
}

bool NavigationInputController::IsValidSlot(size_t slot) const {
    return slot < pressed_.size();
}

}  // namespace page_navigation
