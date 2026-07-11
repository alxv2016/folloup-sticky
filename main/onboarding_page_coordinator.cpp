#include "onboarding_page_coordinator.h"

#include <array>

#include "asset_manifest.h"
#include "project_assets.h"

namespace {

using page_navigation::NavigationItemRole;
using epaper_ui::OnboardingControl;

struct Slide {
    const char* title;
    const char* body;
    EmbeddedImageId image;
};

constexpr std::array<Slide, 6> kSlides = {{
    {"Welcome to Folloup",
     "Your pocket voice notebook. Capture thoughts out loud and let Folloup keep them organized.",
     EmbeddedImageId::kSlide1},
    {"Capture in a tap",
     "Press the mic to record a note, an idea, or a task. Everything is saved straight to the SD "
     "card. Double-press to lock the screen.",
     EmbeddedImageId::kSlide2},
    {"Navigate with keys",
     "Key 1 selects, key 2 navigates up, and key 3 navigates down. Hold key 3 to exit certain "
     "components.",
     EmbeddedImageId::kSlide3},
    {"Sleep & power",
     "The device sleeps when inactive. Hold keys 1 and 2 to shut it down; press and hold key 1 to "
     "turn it on.",
     EmbeddedImageId::kSlide4},
    {"Summaries with Gemini",
     "Connect Gemini and let Folloup transcribe your recordings and summarize your day for you.",
     EmbeddedImageId::kSlide5},
    {"Notes, Todos & Follow-ups",
     "Recordings are grouped by day. Browse them as Notes, mark tasks as Todos, and pin anything as "
     "a follow-up.",
     EmbeddedImageId::kSlide6},
}};

NavigationItemRole RoleForControl(OnboardingControl control)
{
    switch (control) {
        case OnboardingControl::kClose:
            return NavigationItemRole::kOnboardingPageClose;
        case OnboardingControl::kPrev:
            return NavigationItemRole::kOnboardingPagePrev;
        case OnboardingControl::kNext:
            return NavigationItemRole::kOnboardingPageNext;
        case OnboardingControl::kNone:
        default:
            return NavigationItemRole::kUnknown;
    }
}

}  // namespace

OnboardingPageCoordinator::OnboardingPageCoordinator() = default;

int OnboardingPageCoordinator::slide_count() const
{
    return static_cast<int>(kSlides.size());
}

void OnboardingPageCoordinator::PrepareForShow()
{
    active_index_ = 0;
    focus_.Configure(navigation_model_.item_count, 0);
    FocusFirstSelectable();
}

epaper_ui::OnboardingControl OnboardingPageCoordinator::FocusedControl() const
{
    if (IsRoleFocused(NavigationItemRole::kOnboardingPageClose)) {
        return OnboardingControl::kClose;
    }
    if (IsRoleFocused(NavigationItemRole::kOnboardingPagePrev)) {
        return OnboardingControl::kPrev;
    }
    if (IsRoleFocused(NavigationItemRole::kOnboardingPageNext)) {
        return OnboardingControl::kNext;
    }
    return OnboardingControl::kNone;
}

bool OnboardingPageCoordinator::ControlSelectable(OnboardingControl control) const
{
    switch (control) {
        case OnboardingControl::kPrev:
            return !PrevDisabled();
        case OnboardingControl::kNext:
            return !NextDisabled();
        case OnboardingControl::kClose:
            return ShowClose();  // only on the last slide
        case OnboardingControl::kNone:
        default:
            return false;
    }
}

void OnboardingPageCoordinator::FocusRole(NavigationItemRole role)
{
    const int index = navigation_model_.IndexOfRole(role);
    if (index >= 0) {
        focus_.SetIndex(index);
    }
}

void OnboardingPageCoordinator::FocusFirstSelectable()
{
    // Nav order is [Close, Prev, Next]; pick the first control that can currently be focused.
    for (const OnboardingControl control :
         {OnboardingControl::kClose, OnboardingControl::kPrev, OnboardingControl::kNext}) {
        if (ControlSelectable(control)) {
            FocusRole(RoleForControl(control));
            return;
        }
    }
}

bool OnboardingPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

bool OnboardingPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    const int step = delta > 0 ? 1 : -1;
    const int count = navigation_model_.item_count;
    for (int attempt = 0; attempt < count; ++attempt) {
        if (!focus_.Move(step)) {
            return false;
        }
        if (ControlSelectable(FocusedControl())) {
            return true;  // landed on a focusable control
        }
    }
    return false;  // no other focusable control to move to
}

bool OnboardingPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

void OnboardingPageCoordinator::EnsureFocusEnabled()
{
    const OnboardingControl focused = FocusedControl();
    if (ControlSelectable(focused)) {
        return;
    }
    // Reaching the last slide: Next disables and Close appears -> land on Close (the primary CTA).
    if (focused == OnboardingControl::kNext && ShowClose()) {
        FocusRole(NavigationItemRole::kOnboardingPageClose);
        return;
    }
    // Reaching the first slide: Prev disables -> land on Next.
    if (focused == OnboardingControl::kPrev && !NextDisabled()) {
        FocusRole(NavigationItemRole::kOnboardingPageNext);
        return;
    }
    FocusFirstSelectable();
}

bool OnboardingPageCoordinator::NextSlide()
{
    if (NextDisabled()) {
        return false;
    }
    ++active_index_;
    EnsureFocusEnabled();
    return true;
}

bool OnboardingPageCoordinator::PrevSlide()
{
    if (PrevDisabled()) {
        return false;
    }
    --active_index_;
    EnsureFocusEnabled();
    return true;
}

bool OnboardingPageCoordinator::FocusControl(OnboardingControl control)
{
    const NavigationItemRole role = RoleForControl(control);
    if (role == NavigationItemRole::kUnknown || !ControlSelectable(control)) {
        return false;
    }
    const int index = navigation_model_.IndexOfRole(role);
    if (index < 0) {
        return false;
    }
    return focus_.SetIndex(index);
}

epaper_ui::OnboardingPageState OnboardingPageCoordinator::BuildState() const
{
    epaper_ui::OnboardingPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.carousel.slide_count = slide_count();
    state.carousel.active_index = active_index_;
    state.carousel.show_close = ShowClose();
    state.carousel.close_selected = IsRoleFocused(NavigationItemRole::kOnboardingPageClose);
    state.carousel.prev_selected = IsRoleFocused(NavigationItemRole::kOnboardingPagePrev);
    state.carousel.next_selected = IsRoleFocused(NavigationItemRole::kOnboardingPageNext);

    const Slide& slide = kSlides[static_cast<size_t>(active_index_)];
    state.slide_title = slide.title;
    state.slide_body = slide.body;
    state.slide_image = project_assets::GetImage(slide.image);
    return state;
}
