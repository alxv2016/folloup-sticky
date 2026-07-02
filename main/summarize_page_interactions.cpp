#include "summarize_page_interactions.h"

namespace summarize_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(SummarizePageCoordinator& coordinator, bool gemini_ready)
{
    ActivateResult result = {};
    result.handled = true;
    result.play_activate_cue = true;

    if (coordinator.IsRoleFocused(NavigationItemRole::kSummarizePageSegmentControl)) {
        result.intent = ActivateIntent::kToggleSegment;
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kSummarizePageScrollContainer)) {
        if (coordinator.scroll_container_active()) {
            // Already scrolling; OK is inert (DOWN double-click exits).
            result.play_activate_cue = false;
            return result;
        }
        result.intent = ActivateIntent::kEnterScroll;
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kSummarizePageGetSummaryButton)) {
        if (!gemini_ready) {
            // Inline hint already tells the user to connect Gemini; the tap is a no-op.
            result.play_activate_cue = false;
            return result;
        }
        result.intent = coordinator.selected_segment_index() == 0
                            ? ActivateIntent::kRequestNotesSummary
                            : ActivateIntent::kRequestTodosSummary;
        return result;
    }

    switch (coordinator.FocusedRole()) {
        case NavigationItemRole::kFooterHome:
            result.intent = ActivateIntent::kShowHome;
            break;
        case NavigationItemRole::kFooterSettings:
            result.intent = ActivateIntent::kShowSettings;
            break;
        case NavigationItemRole::kFooterWifi:
            result.intent = ActivateIntent::kShowWifi;
            break;
        case NavigationItemRole::kFooterTime:
            result.intent = ActivateIntent::kShowTime;
            break;
        default:
            result.handled = false;
            result.play_activate_cue = false;
            break;
    }
    return result;
}

void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks)
{
    switch (result.intent) {
        case ActivateIntent::kShowHome:
            if (callbacks.show_home) {
                callbacks.show_home();
            }
            break;
        case ActivateIntent::kShowSettings:
            if (callbacks.show_settings) {
                callbacks.show_settings();
            }
            break;
        case ActivateIntent::kShowWifi:
            if (callbacks.show_wifi) {
                callbacks.show_wifi();
            }
            break;
        case ActivateIntent::kShowTime:
            if (callbacks.show_time) {
                callbacks.show_time();
            }
            break;
        case ActivateIntent::kToggleSegment:
            if (callbacks.toggle_segment) {
                callbacks.toggle_segment();
            }
            break;
        case ActivateIntent::kEnterScroll:
            if (callbacks.enter_scroll) {
                callbacks.enter_scroll();
            }
            break;
        case ActivateIntent::kRequestNotesSummary:
            if (callbacks.request_notes_summary) {
                callbacks.request_notes_summary();
            }
            break;
        case ActivateIntent::kRequestTodosSummary:
            if (callbacks.request_todos_summary) {
                callbacks.request_todos_summary();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(SummarizePageCoordinator& coordinator, int delta)
{
    FocusMoveResult result = {};
    if (!coordinator.MoveFocus(delta)) {
        return result;
    }
    result.handled = true;
    result.play_navigation_cue = true;
    result.apply_page_state = true;
    result.sync_footer_projection = true;
    return result;
}

}  // namespace summarize_page_interactions
