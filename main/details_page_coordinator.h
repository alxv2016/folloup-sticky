#ifndef DETAILS_PAGE_COORDINATOR_H_
#define DETAILS_PAGE_COORDINATOR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "epaper_ui/details_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"

enum class DetailsPageSource : uint8_t {
    kUnknown = 0,
    kNotes,
    kTodos,
    kFollowUp,
};

// Owns the Details page: it shows one recording's header + transcript in a scroll container with a
// Back button. Focus roves scroll/back/footer; entering the scroll container captures UP/DOWN.
class DetailsPageCoordinator {
public:
    DetailsPageCoordinator();

    // Stash the target recording before the page is shown (the archive read happens in Show()).
    void QueueShow(const std::string& recording_id, DetailsPageSource source_page);
    void Show(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void RefreshFromArchive(
        const std::vector<recording_archive_service::RecordingEntry>& recordings);

    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;
    bool EnterScrollContainer();
    bool ExitScrollContainer();

    epaper_ui::DetailsPageState BuildState() const;

    bool scroll_container_active() const { return scroll_container_active_; }
    DetailsPageSource source_page() const { return source_page_; }
    const std::string& recording_id() const { return recording_id_; }

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    const recording_archive_service::RecordingEntry* FindEntry(
        const std::vector<recording_archive_service::RecordingEntry>& recordings) const;
    void ApplyEntry(const recording_archive_service::RecordingEntry& entry);
    void ResetScrollPosition() { scroll_position_percent_ = 0; }
    // Rebuilds the navigation model to add/drop the Transcribe button as has_transcript_ changes,
    // re-homing focus on the scroll container when the set of controls changes.
    void UpdateNavigationModel();

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildDetailsPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    bool scroll_container_active_ = false;
    int scroll_position_percent_ = 0;
    std::string pending_recording_id_ = {};
    DetailsPageSource pending_source_page_ = DetailsPageSource::kUnknown;
    std::string recording_id_ = {};
    DetailsPageSource source_page_ = DetailsPageSource::kUnknown;
    std::string title_text_ = "Details";
    epaper_ui::ListItemHeaderState recording_header_ = {};
    std::string transcript_text_ = {};
    bool has_transcript_ = false;
};

#endif  // DETAILS_PAGE_COORDINATOR_H_
