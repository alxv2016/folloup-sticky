#ifndef EPAPER_UI_VIBE_CARD_H_
#define EPAPER_UI_VIBE_CARD_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/button_icon.h"
#include "epaper_ui/list_item_header.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/tag.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

// The card surfaces one idea at a time with a row of actions. Audio playback is not
// supported on this device, so the "Listen" action from the reference design is dropped:
// only Refresh (shuffle), Close (delete) and Check (pin as follow-up) remain.
enum class VibeCardActionSelection : int {
    kNone = -1,
    kRefresh = 0,
    kClose = 1,
    kCheck = 2,
    // Transcribe an audio-only idea. Rendered as a separate Star button on the card's right
    // edge (not part of the bottom action row), shown only when show_transcribe_action is set.
    kTranscribe = 3,
};

// Count of buttons in the bottom action row (Refresh / Close / Check). The transcribe Star is
// a separate right-edge button and is not included here.
inline constexpr int kVibeCardActionCount = 3;

struct VibeCardFooterState {
    VibeCardActionSelection selected_action = VibeCardActionSelection::kNone;

    bool operator==(const VibeCardFooterState& other) const = default;
};

struct VibeCardState {
    bool focused = false;
    bool empty = false;
    std::string tag_text = {};
    ListItemHeaderState header = {};
    std::string body_text = {};
    const EmbeddedImageAsset* empty_state_icon_asset = nullptr;
    std::string empty_state_message = {};
    VibeCardFooterState footer = {};
    // Show the right-edge Star (transcribe) button. Set for audio-only ideas that have no
    // transcript yet; tapping it re-runs transcription.
    bool show_transcribe_action = false;

    bool operator==(const VibeCardState& other) const = default;
};

struct VibeCardStyle {
    uint8_t background_color = design::vibe_card::kBackgroundColor;
    uint8_t border_color = design::vibe_card::kBorderColor;
    uint8_t empty_state_background_color = design::vibe_card::kEmptyStateBackgroundColor;
    uint8_t body_outline_color = design::color::kWhite;
    uint8_t inactive_focus_ring_color = design::color::kFocusRingInactive;
    uint8_t active_focus_ring_color = design::color::kFocusRingActive;
    uint8_t focus_gap_color = design::color::kFocusGap;
    int width = 0;
    // Fixed card height when > 0 (the page stretches the card to fill the space between the
    // heading and the bottom-anchored progress/message block); otherwise the card sizes to its
    // content, clamped to min_height. The action row is always pinned to the card's bottom.
    int height = 0;
    int min_height = design::vibe_card::kMinHeight;
    int padding = design::vibe_card::kPadding;
    int border_thickness = design::vibe_card::kBorderThickness;
    int tag_header_gap = design::vibe_card::kTagHeaderGap;
    int header_body_gap = design::vibe_card::kHeaderBodyGap;
    int footer_gap = design::vibe_card::kFooterGap;
    int body_line_gap = design::vibe_card::kBodyLineGap;
    int body_outline_thickness = design::vibe_card::kBodyOutlineThickness;
    int footer_icon_slot_size = design::vibe_card::kFooterIconSlotSize;
    int footer_button_gap = design::vibe_card::kFooterButtonGap;
    int empty_state_gap = design::empty_state::kGap;
    int empty_state_icon_size = design::empty_state::kIconSize;
    int focus_ring_thickness = design::toggle::kFocusRingThickness;
    int focus_gap = design::toggle::kFocusGap;
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
    design::TypographyRole body_role = design::TypographyRole::kBodyLarge;
    uint8_t body_color = design::color::kBlack;
    ButtonIconStyle footer_button = {
        .background_color = design::vibe_card::kFooterButtonBackgroundColor,
        .selected_background_color = design::color::kBlack,
        .border_color = design::color::kBlack,
        .icon_color = design::color::kBlack,
        .selected_icon_color = design::color::kWhite,
        .size = design::global_footer::kButtonSize,
        .icon_size = design::global_footer::kIconSize,
    };
    design::TypographyRole empty_state_body_role = design::TypographyRole::kLabelSmall;
    uint8_t empty_state_body_color = design::color::kBlack;
};

// Card bounds (its full height depends on wrapped body text, so pass the resolved width).
UiRect VibeCardBounds(int origin_x,
                      int origin_y,
                      int portrait_width,
                      const VibeCardState& state,
                      const VibeCardStyle& style);
// Hit-test one footer action button. Returns false / kNone when the point misses.
bool HitTestVibeCardAction(int origin_x,
                           int origin_y,
                           int portrait_width,
                           const VibeCardState& state,
                           const VibeCardStyle& style,
                           int x,
                           int y,
                           VibeCardActionSelection* action);
void DrawVibeCard(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const VibeCardState& state,
                  const VibeCardStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_VIBE_CARD_H_
