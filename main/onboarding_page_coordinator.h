#ifndef ONBOARDING_PAGE_COORDINATOR_H_
#define ONBOARDING_PAGE_COORDINATOR_H_

#include "epaper_ui/onboarding_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"

// Owns the onboarding carousel: which slide is active and which control (Close / Prev / Next) is
// focused. Focus roves the three controls (skipping a disabled arrow at the ends); Prev/Next change
// the slide, Close dismisses (handled by the runtime/app_shell).
class OnboardingPageCoordinator {
public:
    OnboardingPageCoordinator();

    // (Re)enter the page: first slide, focus the Close button.
    void PrepareForShow();

    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;
    epaper_ui::OnboardingControl FocusedControl() const;

    // Advance / rewind the active slide. Returns true when the slide actually changed.
    bool NextSlide();
    bool PrevSlide();
    // Focus a control by touch. Returns true if it changed focus.
    bool FocusControl(epaper_ui::OnboardingControl control);

    epaper_ui::OnboardingPageState BuildState() const;

    int active_index() const { return active_index_; }
    int slide_count() const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    bool PrevDisabled() const { return active_index_ <= 0; }
    bool NextDisabled() const { return active_index_ >= slide_count() - 1; }
    // Close is only shown (and focusable) on the last slide.
    bool ShowClose() const { return active_index_ >= slide_count() - 1; }
    // A control can be focused only when it is enabled and visible.
    bool ControlSelectable(epaper_ui::OnboardingControl control) const;
    void FocusRole(page_navigation::NavigationItemRole role);
    void FocusFirstSelectable();
    // After a slide change, move focus off a control that just became unavailable.
    void EnsureFocusEnabled();

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildOnboardingPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    int active_index_ = 0;
};

#endif  // ONBOARDING_PAGE_COORDINATOR_H_
