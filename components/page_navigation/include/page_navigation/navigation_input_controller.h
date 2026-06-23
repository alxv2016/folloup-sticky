#ifndef PAGE_NAVIGATION_NAVIGATION_INPUT_CONTROLLER_H_
#define PAGE_NAVIGATION_NAVIGATION_INPUT_CONTROLLER_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace page_navigation {

constexpr uint32_t kDefaultNavigationHoldStartMs = 500;
constexpr uint32_t kDefaultNavigationHoldRepeatIntervalMs = 160;
constexpr size_t kDefaultNavigationInputSlotCount = 2;

class NavigationInputController {
public:
    explicit NavigationInputController(
        uint32_t hold_start_ms = kDefaultNavigationHoldStartMs,
        uint32_t hold_repeat_interval_ms = kDefaultNavigationHoldRepeatIntervalMs);

    void BeginPress(size_t slot);
    void EndPress(size_t slot);
    void ResetHold(size_t slot);

    uint32_t CurrentGeneration(size_t slot) const;
    bool IsCurrentPress(size_t slot, uint32_t generation) const;
    bool ShouldHandleHold(size_t slot, uint32_t generation, uint32_t pressed_time_ms);

private:
    bool IsValidSlot(size_t slot) const;

    uint32_t hold_start_ms_;
    uint32_t hold_repeat_interval_ms_;
    std::array<std::atomic_bool, kDefaultNavigationInputSlotCount> pressed_ = {};
    std::array<std::atomic<uint32_t>, kDefaultNavigationInputSlotCount> generation_ = {};
    std::array<uint32_t, kDefaultNavigationInputSlotCount> last_hold_pressed_time_ms_ = {};
};

}  // namespace page_navigation

#endif  // PAGE_NAVIGATION_NAVIGATION_INPUT_CONTROLLER_H_
