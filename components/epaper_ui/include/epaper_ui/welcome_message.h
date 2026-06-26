#ifndef EPAPER_UI_WELCOME_MESSAGE_H_
#define EPAPER_UI_WELCOME_MESSAGE_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/current_date.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// The dashboard header: the current date on top, then a large greeting that word-wraps to
// the available width with a trailing star icon after the last line.
struct WelcomeMessageState {
    CurrentDateState current_date = {};
    std::string title_text = {};

    bool operator==(const WelcomeMessageState& other) const = default;
};

struct WelcomeMessageStyle {
    CurrentDateStyle current_date = {};
    design::TypographyRole title_role = design::TypographyRole::kDisplay;
    uint8_t title_color = design::color::kBlack;
    int title_max_width = 0;  // 0 = no wrapping (single line)
    int section_gap = design::welcome_message::kSectionGap;
    int icon_gap = design::welcome_message::kIconGap;
    int icon_y_offset = design::welcome_message::kIconYOffset;
    uint8_t icon_color = design::color::kBlack;
    bool show_icon = true;
};

// Returns the title variant text for a rotating greeting, indexed by seed.
std::string WelcomeMessageTitle(uint32_t seed);
int WelcomeMessageTitleCount();

UiRect WelcomeMessageBounds(int origin_x,
                            int origin_y,
                            const WelcomeMessageState& state,
                            const WelcomeMessageStyle& style);
void DrawWelcomeMessage(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        int origin_x,
                        int origin_y,
                        const WelcomeMessageState& state,
                        const WelcomeMessageStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_WELCOME_MESSAGE_H_
