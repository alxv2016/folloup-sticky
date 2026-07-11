#include "epaper_ui/onboarding_page.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "asset_types.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH2;
constexpr auto kBodyRole = design::TypographyRole::kBody;
constexpr int kTitleBodyGap = design::spacing::k16;
constexpr int kBodyLineGap = design::spacing::k4;
constexpr int kBodyImageGap = design::spacing::k24;

CarouselStyle Style()
{
    return {};
}

}  // namespace

OnboardingControl HitTestOnboarding(int portrait_width,
                                    int portrait_height,
                                    const OnboardingPageState& state,
                                    int x,
                                    int y)
{
    const CarouselStyle style = Style();
    if (HitTestCarouselClose(portrait_width, portrait_height, state.carousel, style, x, y)) {
        return OnboardingControl::kClose;
    }
    if (HitTestCarouselPrev(portrait_width, portrait_height, state.carousel, style, x, y)) {
        return OnboardingControl::kPrev;
    }
    if (HitTestCarouselNext(portrait_width, portrait_height, state.carousel, style, x, y)) {
        return OnboardingControl::kNext;
    }
    return OnboardingControl::kNone;
}

CarouselControlRects OnboardingControlBounds(int portrait_width,
                                             int portrait_height,
                                             const OnboardingPageState& state)
{
    return CarouselControlBounds(portrait_width, portrait_height, state.carousel, Style());
}

void DrawOnboardingPage(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        const OnboardingPageState& state,
                        const StatusBarState& status_bar_state)
{
    if (framebuffer == nullptr) {
        return;
    }

    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     {0, 0, portrait_width, portrait_height}, design::color::kWhite);
    DrawStatusBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  status_bar_state);

    const CarouselStyle style = Style();
    const UiRect content = CarouselContentBounds(portrait_width, portrait_height, style);

    // Active slide: title over a wrapped body paragraph, top-aligned in the content slot.
    int cursor_y = content.y;
    if (!state.slide_title.empty() && !content.IsEmpty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           content.x, cursor_y, state.slide_title, kTitleRole,
                           design::color::kTextPrimary);
        cursor_y += LineHeight(kTitleRole) + kTitleBodyGap;
    }
    if (!state.slide_body.empty() && !content.IsEmpty()) {
        const int body_line_height = LineHeight(kBodyRole);
        const std::vector<std::string> lines =
            WrapTextToWidth(kBodyRole, state.slide_body, content.width);
        for (const std::string& line : lines) {
            if (cursor_y + body_line_height > content.bottom()) {
                break;
            }
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               content.x, cursor_y, line, kBodyRole, design::color::kTextPrimary);
            cursor_y += body_line_height + kBodyLineGap;
        }
    }

    // Slide illustration pinned to the bottom of the content slot (which already sits 16px above the
    // carousel controls), aspect-fit into the remaining width/height and centered horizontally.
    if (state.slide_image != nullptr && state.slide_image->width > 0 &&
        state.slide_image->height > 0 && !content.IsEmpty()) {
        cursor_y += kBodyImageGap;  // minimum gap below the body
        const int avail_w = content.width;
        const int avail_h = std::max(0, content.bottom() - cursor_y);
        if (avail_w > 0 && avail_h > 0) {
            const int iw = state.slide_image->width;
            const int ih = state.slide_image->height;
            int draw_w = avail_w;
            int draw_h = static_cast<int>(static_cast<int64_t>(draw_w) * ih / iw);
            if (draw_h > avail_h) {
                draw_h = avail_h;
                draw_w = static_cast<int>(static_cast<int64_t>(draw_h) * iw / ih);
            }
            const int image_x = content.x + std::max(0, (avail_w - draw_w) / 2);
            const int image_y = content.bottom() - draw_h;  // bottom-aligned -> 16px above controls
            DrawScaledPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width,
                                        portrait_height, {image_x, image_y, draw_w, draw_h},
                                        state.slide_image, design::color::kBlack);
        }
    }

    DrawCarousel(framebuffer, raw_width, raw_height, portrait_width, portrait_height, state.carousel,
                 style);
}

}  // namespace epaper_ui
