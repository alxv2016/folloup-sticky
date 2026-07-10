#ifndef EPAPER_UI_ONBOARDING_PAGE_H_
#define EPAPER_UI_ONBOARDING_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/carousel.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

// First-run onboarding: a carousel of slides below the status bar. This composer draws the active
// slide's title + body into the carousel's content slot and the carousel chrome around it.
struct OnboardingPageState {
    int navigation_focus_index = -1;
    CarouselState carousel = {};
    std::string slide_title = {};
    std::string slide_body = {};

    bool operator==(const OnboardingPageState& other) const = default;
};

enum class OnboardingControl : uint8_t {
    kNone = 0,
    kClose,
    kPrev,
    kNext,
};

// Resolve a touch to one of the carousel controls (kNone if it hit nothing / a disabled control).
OnboardingControl HitTestOnboarding(int portrait_width,
                                    int portrait_height,
                                    const OnboardingPageState& state,
                                    int x,
                                    int y);

// The resolved control rectangles (same layout the hit-test uses) -- for touch diagnostics.
CarouselControlRects OnboardingControlBounds(int portrait_width,
                                             int portrait_height,
                                             const OnboardingPageState& state);

void DrawOnboardingPage(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        const OnboardingPageState& state,
                        const StatusBarState& status_bar_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_ONBOARDING_PAGE_H_
