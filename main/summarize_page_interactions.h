#ifndef SUMMARIZE_PAGE_INTERACTIONS_H_
#define SUMMARIZE_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "summarize_page_coordinator.h"

namespace summarize_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    kShowWifi,
    kShowTime,
    kToggleSegment,
    kEnterScroll,
    kRequestNotesSummary,
    kRequestTodosSummary,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_settings;
    std::function<void()> show_wifi;
    std::function<void()> show_time;
    std::function<void()> toggle_segment;
    std::function<void()> enter_scroll;
    std::function<void()> request_notes_summary;
    std::function<void()> request_todos_summary;
};

// gemini_ready gates the Get-summary action: when Gemini isn't connected the tap is a no-op
// (the scroll container already shows the "Connect to Gemini" hint).
ActivateResult HandlePrimaryActivate(SummarizePageCoordinator& coordinator, bool gemini_ready);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(SummarizePageCoordinator& coordinator, int delta);

}  // namespace summarize_page_interactions

#endif  // SUMMARIZE_PAGE_INTERACTIONS_H_
