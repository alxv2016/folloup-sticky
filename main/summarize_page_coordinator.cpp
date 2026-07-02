#include "summarize_page_coordinator.h"

#include <algorithm>

namespace {

using page_navigation::NavigationItemRole;

constexpr int kScrollStepPercent = 10;
constexpr const char* kConnectToGeminiMessage = "Connect to Gemini for summaries";
constexpr const char* kEmptyStateMessage = "Summarize your thoughts";

}  // namespace

SummarizePageCoordinator::SummarizePageCoordinator()
{
    PrepareForShow();
}

void SummarizePageCoordinator::PrepareForShow()
{
    segment_control_active_ = false;
    scroll_container_active_ = false;
    segment_focus_.Configure(epaper_ui::kSegmentControlDefaultSegmentCount, selected_segment_index_);
    focus_.Configure(
        navigation_model_.item_count,
        navigation_model_.IndexOfRole(NavigationItemRole::kSummarizePageSegmentControl));
}

bool SummarizePageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }

    // While a control is entered, UP/DOWN drives it (switch segment / scroll). Leaving is the
    // app-wide DOWN double-click gesture, handled by the interactions layer.
    if (segment_control_active_) {
        if (!segment_focus_.Move(delta)) {
            return false;
        }
        selected_segment_index_ = segment_focus_.index();
        return true;
    }
    if (scroll_container_active_) {
        const int current = scroll_position_percent_[static_cast<size_t>(selected_segment_index_)];
        const int next = std::clamp(
            current + (delta > 0 ? kScrollStepPercent : -kScrollStepPercent), 0, 100);
        if (next == current) {
            return false;
        }
        scroll_position_percent_[static_cast<size_t>(selected_segment_index_)] = next;
        return true;
    }
    return focus_.Move(delta);
}

bool SummarizePageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool SummarizePageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

page_navigation::NavigationItemRole SummarizePageCoordinator::FocusedRole() const
{
    const page_navigation::NavigationItemDescriptor* item =
        navigation_model_.ItemAt(focus_.index());
    return item != nullptr ? item->role : NavigationItemRole::kUnknown;
}

bool SummarizePageCoordinator::EnterSegmentControl()
{
    if (segment_control_active_) {
        return false;
    }
    segment_control_active_ = true;
    segment_focus_.Configure(epaper_ui::kSegmentControlDefaultSegmentCount, selected_segment_index_);
    return true;
}

bool SummarizePageCoordinator::ExitSegmentControl()
{
    if (!segment_control_active_) {
        return false;
    }
    selected_segment_index_ = segment_focus_.index();
    segment_control_active_ = false;
    return true;
}

bool SummarizePageCoordinator::EnterScrollContainer()
{
    if (scroll_container_active_) {
        return false;
    }
    scroll_container_active_ = true;
    return true;
}

bool SummarizePageCoordinator::ExitScrollContainer()
{
    if (!scroll_container_active_) {
        return false;
    }
    scroll_container_active_ = false;
    return true;
}

bool SummarizePageCoordinator::SelectSegment(int index)
{
    if (index < 0 || index >= epaper_ui::kSegmentControlDefaultSegmentCount ||
        index == selected_segment_index_) {
        return false;
    }
    selected_segment_index_ = index;
    segment_focus_.Configure(epaper_ui::kSegmentControlDefaultSegmentCount, index);
    return true;
}

epaper_ui::SummarizePageState SummarizePageCoordinator::BuildState(
    bool gemini_ready, const summary_service::Snapshot& summary_snapshot) const
{
    epaper_ui::SummarizePageState state = {};
    state.navigation_focus_index = focus_.index();

    state.segment_control.labels = {"Notes", "Todos", ""};
    state.segment_control.segment_count = epaper_ui::kSegmentControlDefaultSegmentCount;
    state.segment_control.selected_index = selected_segment_index_;
    state.segment_control.focused =
        IsRoleFocused(NavigationItemRole::kSummarizePageSegmentControl) || segment_control_active_;
    state.segment_control.active = segment_control_active_;

    state.scroll_container.content_text = BuildContentText(summary_snapshot);
    state.scroll_container.empty_state_message =
        BuildEmptyStateMessage(gemini_ready, summary_snapshot);
    state.scroll_container.focused =
        IsRoleFocused(NavigationItemRole::kSummarizePageScrollContainer) || scroll_container_active_;
    state.scroll_container.active = scroll_container_active_;
    state.scroll_container.scroll_position_percent =
        scroll_position_percent_[static_cast<size_t>(selected_segment_index_)];

    state.get_summary_button.label_text = "Get summary";
    state.get_summary_button.selected =
        IsRoleFocused(NavigationItemRole::kSummarizePageGetSummaryButton);
    return state;
}

std::string SummarizePageCoordinator::BuildContentText(
    const summary_service::Snapshot& summary_snapshot) const
{
    if (selected_segment_index_ == 0) {
        return summary_snapshot.notes.available ? summary_snapshot.notes.text : std::string();
    }
    return summary_snapshot.todos.available ? summary_snapshot.todos.text : std::string();
}

std::string SummarizePageCoordinator::BuildEmptyStateMessage(
    bool gemini_ready, const summary_service::Snapshot& summary_snapshot) const
{
    if (selected_segment_index_ == 0 && summary_snapshot.notes.available) {
        return {};
    }
    if (selected_segment_index_ == 1 && summary_snapshot.todos.available) {
        return {};
    }
    if (!gemini_ready) {
        return kConnectToGeminiMessage;
    }
    return kEmptyStateMessage;
}
