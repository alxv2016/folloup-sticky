#ifndef VIBE_CHECK_PAGE_COORDINATOR_H_
#define VIBE_CHECK_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/vibe_check_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"

// Owns the Vibe Check page's data and focus. It surfaces one random Idea-tagged recording at
// a time and tracks the "session" (how many ideas there were when the page was entered) so the
// progress bar can read "remaining / initial ideas".
class VibeCheckPageCoordinator {
public:
    VibeCheckPageCoordinator();

    // (Re)enter the page: focus the card, collapse any active-card state, reset the session.
    void PrepareForShow();
    // Reload the idea list from the archive. Keeps the current idea if it still qualifies.
    void RefreshFromArchive(const std::vector<recording_archive_service::RecordingEntry>& entries);

    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;
    page_navigation::NavigationItemRole FocusedRole() const;

    // Card interaction: enter the card (arms the action row), or leave it.
    bool EnterCard();
    // Enter the card with a specific action pre-focused (used when a button is tapped).
    bool EnterCardAtAction(int action_index);
    bool ExitCard();
    bool card_active() const { return card_active_; }
    bool HasIdeas() const;
    epaper_ui::VibeCardActionSelection selected_action() const;

    // Shuffle to a different idea. Returns true when the shown idea actually changed.
    bool RandomizeIdea();
    // Drop the current idea from the in-memory list (caller performs the archive-side action).
    bool RemoveCurrentIdea();
    const std::string& current_recording_id() const { return current_recording_id_; }

    epaper_ui::VibeCheckPageState BuildState() const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    void LoadIdeas(const std::vector<recording_archive_service::RecordingEntry>& entries,
                   bool reset_session);
    void RebuildCardState();
    void ResetSessionIfEmpty();
    void SelectRandomIdea(bool avoid_current);
    const recording_archive_service::RecordingEntry* FindCurrentIdea() const;
    static bool IsIdeaCandidate(const recording_archive_service::RecordingEntry& entry);

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildVibeCheckPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    page_navigation::RovingFocus action_focus_{epaper_ui::kVibeCardActionCount, 0};
    bool card_active_ = false;
    bool session_initialized_ = false;
    size_t initial_idea_count_ = 0;
    std::vector<recording_archive_service::RecordingEntry> ideas_ = {};
    std::string current_recording_id_ = {};
    epaper_ui::VibeCardState card_state_ = {};
    epaper_ui::ProgressBarState progress_state_ = {};
};

#endif  // VIBE_CHECK_PAGE_COORDINATOR_H_
