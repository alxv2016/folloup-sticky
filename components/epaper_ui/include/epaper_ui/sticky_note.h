#ifndef EPAPER_UI_STICKY_NOTE_H_
#define EPAPER_UI_STICKY_NOTE_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/button_icon.h"
#include "epaper_ui/list_item_header.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/tag.h"

namespace epaper_ui {

// A full-page overlay that flips through the Follow-up notes one "sticky" at a time. It overlays the
// page from just below the status bar with a 20px margin on every screen edge, drops a modal-style
// shadow, and wears the vibe card's surface look. Content mirrors the vibe card (tag + header +
// body). The footer carries a Close button plus Prev/Next chevrons (carousel order) on the right and
// an "N/M Stickies" counter on the left. Prev/Next wrap around, so no control is ever disabled.
enum class StickyNoteControl : int {
    kNone = -1,
    kClose = 0,
    kPrev = 1,
    kNext = 2,
};

// Footer controls, left-to-right after the counter: Close, Prev, Next.
inline constexpr int kStickyNoteControlCount = 3;

struct StickyNoteState {
    bool visible = false;
    int active_index = 0;  // 0-based index of the sticky on screen
    int sticky_count = 0;  // total stickies, for the "N/M Stickies" counter
    // Content, mirroring the vibe card.
    std::string tag_text = {};
    ListItemHeaderState header = {};
    std::string body_text = {};
    // The footer control currently pressed / focused (drives its selected styling).
    StickyNoteControl selected_control = StickyNoteControl::kNone;

    bool operator==(const StickyNoteState& other) const = default;
};

struct StickyNoteStyle {
    int screen_margin = design::sticky_note::kScreenMargin;
    int corner_radius = design::sticky_note::kCornerRadius;
    int shadow_offset = design::modal::kShadowOffset;
    int padding = design::vibe_card::kPadding;
    int border_thickness = design::vibe_card::kBorderThickness;
    int tag_header_gap = design::vibe_card::kTagHeaderGap;
    int header_body_gap = design::vibe_card::kHeaderBodyGap;
    int footer_gap = design::vibe_card::kFooterGap;
    int body_line_gap = design::vibe_card::kBodyLineGap;
    int footer_icon_slot_size = design::vibe_card::kFooterIconSlotSize;
    int footer_button_gap = design::vibe_card::kFooterButtonGap;
    uint8_t background_color = design::vibe_card::kBackgroundColor;
    uint8_t border_color = design::vibe_card::kBorderColor;
    uint8_t shadow_color = design::color::kShadow;
    design::TypographyRole body_role = design::TypographyRole::kBodyLarge;
    uint8_t body_color = design::color::kBlack;
    design::TypographyRole counter_role = design::TypographyRole::kLabelSmall;
    uint8_t counter_color = design::color::kBlack;
    TagStyle tag = {
        .background_color = design::color::kBlack,
        .selected_background_color = design::color::kBlack,
        .text_color = design::color::kWhite,
        .selected_text_color = design::color::kWhite,
        .border_color = design::color::kBlack,
    };
    ListItemHeaderStyle header = {
        .background_color = design::vibe_card::kBackgroundColor,
        .selected_background_color = design::vibe_card::kBackgroundColor,
        .text_color = design::color::kBlack,
        .selected_text_color = design::color::kBlack,
        .icon_color = design::color::kBlack,
        .selected_icon_color = design::color::kBlack,
        .divider_color = design::color::kBlack,
        .selected_divider_color = design::color::kBlack,
    };
    ButtonIconStyle footer_button = {
        .background_color = design::vibe_card::kFooterButtonBackgroundColor,
        .selected_background_color = design::color::kBlack,
        .border_color = design::color::kBlack,
        .icon_color = design::color::kBlack,
        .selected_icon_color = design::color::kWhite,
        .size = design::global_footer::kButtonSize,
        .icon_size = design::global_footer::kIconSize,
    };
};

// The overlay panel: 20px inside every screen edge, its top just below the status bar. Empty when
// the sticky note is not visible.
UiRect StickyNotePanelBounds(int portrait_width, int portrait_height, const StickyNoteStyle& style);
// The content region the tag/header/body fill (inside the padding, above the footer row).
UiRect StickyNoteContentBounds(int portrait_width,
                               int portrait_height,
                               const StickyNoteStyle& style);

// Resolved on-screen rectangles of the three footer controls (for hit-testing / wiring).
struct StickyNoteControlRects {
    UiRect close = {};
    UiRect prev = {};
    UiRect next = {};
};
StickyNoteControlRects StickyNoteControlBounds(int portrait_width,
                                               int portrait_height,
                                               const StickyNoteStyle& style);

// Hit-test the footer controls. Returns false / kNone when the point misses every control.
bool HitTestStickyNoteControl(int portrait_width,
                              int portrait_height,
                              const StickyNoteState& state,
                              const StickyNoteStyle& style,
                              int x,
                              int y,
                              StickyNoteControl* control);

// Draw the whole overlay: shadow, panel, content (tag / header / body) and footer.
void DrawStickyNote(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    const StickyNoteState& state,
                    const StickyNoteStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_STICKY_NOTE_H_
