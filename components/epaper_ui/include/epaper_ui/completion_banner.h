#ifndef EPAPER_UI_COMPLETION_BANNER_H_
#define EPAPER_UI_COMPLETION_BANNER_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"
#include "project_assets.h"

namespace epaper_ui {

// A celebration/announcement banner: an icon in a circle on the left, a wrapped message to
// its right, inside a rounded surface. Shown in place of the progress widget when active.
struct CompletionBannerState {
    EmbeddedIconId icon = EmbeddedIconId::kStar;
    std::string message_text = {};

    bool operator==(const CompletionBannerState& other) const = default;
};

struct CompletionBannerStyle {
    design::TypographyRole role = design::TypographyRole::kBody;
    uint8_t background_color = design::color::kGrayLight;
    uint8_t text_color = design::color::kBlack;
    uint8_t icon_background_color = design::color::kWhite;
    uint8_t icon_color = design::color::kBlack;
    int width = 0;
    int min_height = design::completion_banner::kMinHeight;
    int padding = design::completion_banner::kPadding;
    int content_gap = design::completion_banner::kContentGap;
    int icon_container_size = design::completion_banner::kIconContainerSize;
    int icon_size = design::completion_banner::kIconSize;
    // Asymmetric corners: the left side is a full pill (fully rounded left corners) and the right
    // side uses a small radius, matching the followup design.
    int left_corner_radius = design::completion_banner::kLeftCornerRadius;
    int right_corner_radius = design::completion_banner::kRightCornerRadius;
};

UiRect CompletionBannerBounds(int origin_x,
                              int origin_y,
                              const CompletionBannerState& state,
                              const CompletionBannerStyle& style);
void DrawCompletionBanner(uint8_t* framebuffer,
                          int raw_width,
                          int raw_height,
                          int portrait_width,
                          int portrait_height,
                          int origin_x,
                          int origin_y,
                          const CompletionBannerState& state,
                          const CompletionBannerStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_COMPLETION_BANNER_H_
