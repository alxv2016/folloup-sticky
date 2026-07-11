#ifndef DESIGN_TOKENS_H_
#define DESIGN_TOKENS_H_

#include <cstdint>

namespace design {

enum class TypographyRole : uint8_t {
    kLabelMedium,
    kLabelMediumBlack,
    kLabelMediumBold,
    kLabelLarge,
    kLabelLargeBlack,
    kLabelLargeBold,
    kLabelSmallBlack,
    kLabelSmallBold,
    kStatusBarLabel,
    kHeadingH1,
    kHeadingH2,
    kHeadingH3,
    kDisplay,
    kLabelXL,
    kDetail,
    kBody,
    kBodyLarge,
    kInputValue,
    kLabelSmall,
    kDisplayXL,
};

namespace spacing {

inline constexpr int k2 = 2;
inline constexpr int k4 = 4;
inline constexpr int k8 = 8;
inline constexpr int k12 = 12;
inline constexpr int k14 = 14;
inline constexpr int k16 = 16;
inline constexpr int k20 = 20;
inline constexpr int k24 = 24;
inline constexpr int k32 = 32;
inline constexpr int k40 = 40;
inline constexpr int k48 = 48;
inline constexpr int k56 = 56;
inline constexpr int k64 = 64;
inline constexpr int k68 = 68;
inline constexpr int k72 = 72;

}  // namespace spacing

namespace typography {

inline constexpr int k22 = 22;
inline constexpr int k26 = 26;
inline constexpr int k32 = 32;
inline constexpr int k38 = 38;
inline constexpr int k46 = 46;
inline constexpr int k55 = 55;
inline constexpr int k165 = 165;

}  // namespace typography

namespace font_weight {

inline constexpr int kSemibold = 600;
inline constexpr int kBold = 700;
inline constexpr int kBlack = 900;

}  // namespace font_weight

struct TypographySpec {
    int size = typography::k22;
    int weight = font_weight::kSemibold;
    int scale = 1;
    int line_height_override = 0;
};

constexpr TypographySpec TypographySpecForRole(TypographyRole role)
{
    switch (role) {
        case TypographyRole::kLabelMedium:
            return {typography::k26, font_weight::kSemibold, 1, 0};
        case TypographyRole::kLabelMediumBlack:
            return {typography::k26, font_weight::kBlack, 1, 0};
        case TypographyRole::kLabelMediumBold:
            return {typography::k26, font_weight::kBold, 1, 0};
        case TypographyRole::kLabelLarge:
            return {typography::k32, font_weight::kSemibold, 1, 0};
        case TypographyRole::kLabelLargeBlack:
            return {typography::k32, font_weight::kBlack, 1, 0};
        case TypographyRole::kLabelLargeBold:
            return {typography::k32, font_weight::kBold, 1, 0};
        case TypographyRole::kLabelSmallBlack:
            return {typography::k22, font_weight::kBlack, 1, 0};
        case TypographyRole::kLabelSmallBold:
            return {typography::k22, font_weight::kBold, 1, 0};
        case TypographyRole::kStatusBarLabel:
            return {typography::k22, font_weight::kBold, 1, 0};
        case TypographyRole::kHeadingH1:
            return {typography::k38, font_weight::kBlack, 1, 0};
        case TypographyRole::kHeadingH2:
            return {typography::k32, font_weight::kBold, 1, 0};
        case TypographyRole::kHeadingH3:
            return {typography::k26, font_weight::kBold, 1, 0};
        case TypographyRole::kDisplay:
            return {typography::k46, font_weight::kBlack, 1, 0};
        case TypographyRole::kLabelXL:
            return {typography::k38, font_weight::kBlack, 1, 0};
        case TypographyRole::kDetail:
        case TypographyRole::kBody:
        case TypographyRole::kInputValue:
        case TypographyRole::kLabelSmall:
            return {typography::k22, font_weight::kSemibold, 1, 0};
        case TypographyRole::kBodyLarge:
            return {typography::k26, font_weight::kSemibold, 1, 0};
        case TypographyRole::kDisplayXL:
            return {typography::k165, font_weight::kBlack, 1, 0};
    }

    return {};
}

namespace icon {

inline constexpr int k12 = 12;
inline constexpr int k16 = 16;
inline constexpr int k20 = 20;
inline constexpr int k24 = 24;
inline constexpr int k32 = 32;
inline constexpr int k34 = 34;
inline constexpr int k36 = 36;
inline constexpr int k44 = 44;
inline constexpr int k64 = 64;

}  // namespace icon

namespace color {

inline constexpr uint8_t kBlack = 0x00;
inline constexpr uint8_t kGrayDark = 0x55;
inline constexpr uint8_t kGrayLight = 0xAA;
inline constexpr uint8_t kWhite = 0xFF;

// Canonical four-step e-paper grayscale ramp used by UI components.
inline constexpr uint8_t kGray1 = kBlack;
inline constexpr uint8_t kGray2 = kGrayDark;
inline constexpr uint8_t kGray3 = kGrayLight;
inline constexpr uint8_t kGray4 = kWhite;

inline constexpr uint8_t kGrayMid = kGray3;

inline constexpr uint8_t kSurfaceBase = kWhite;
inline constexpr uint8_t kSurfaceRaised = kGray3;
inline constexpr uint8_t kSurfaceEmphasis = kGray2;
inline constexpr uint8_t kSurfaceInverse = kBlack;

inline constexpr uint8_t kTextPrimary = kBlack;
inline constexpr uint8_t kTextSecondary = kGray2;
inline constexpr uint8_t kTextInverse = kWhite;

inline constexpr uint8_t kBorderSubtle = kGray3;
inline constexpr uint8_t kBorderStrong = kBlack;

inline constexpr uint8_t kFocusRingInactive = kBlack;
inline constexpr uint8_t kFocusRingActive = kGray2;
inline constexpr uint8_t kFocusGap = kWhite;
inline constexpr uint8_t kOutlineKnockout = kWhite;

inline constexpr uint8_t kScrollbarTrack = kWhite;
inline constexpr uint8_t kScrollbarThumb = kGray3;
inline constexpr uint8_t kScrollbarThumbDisabled = kGray2;
inline constexpr uint8_t kScrollbarThumbActive = kBlack;

inline constexpr uint8_t kShadow = kGray3;

}  // namespace color

namespace status_bar {

inline constexpr int kHeight = 44;
inline constexpr int kTopPadding = 1;
inline constexpr int kBottomPadding = 1;
inline constexpr int kSidePadding = spacing::k16;
inline constexpr int kItemGap = spacing::k12;
inline constexpr int kIconTextGap = spacing::k4;
inline constexpr int kDividerGap = spacing::k8;
inline constexpr int kBaselineOffset = 32;
inline constexpr int kPreferredIconSize = icon::k36;
inline constexpr int kStrokeThickness = 2;
inline constexpr uint8_t kBackgroundColor = color::kSurfaceRaised;

}  // namespace status_bar

namespace lock_screen {

inline constexpr int kTopPadding = spacing::k16;
inline constexpr int kSidePadding = spacing::k16;
inline constexpr int kIndicatorRowHeight = icon::k36;
inline constexpr int kIndicatorItemGap = spacing::k12;
inline constexpr int kIndicatorTextGap = spacing::k4;
inline constexpr int kIndicatorStrokeThickness = 2;
inline constexpr int kContentTop = spacing::k72 + spacing::k48;
inline constexpr int kTimeLineHeight = 130;
inline constexpr int kTimeGap = spacing::k4;
inline constexpr int kDateGap = spacing::k64;
inline constexpr int kWeekdayDateGap = spacing::k2;

}  // namespace lock_screen

namespace current_date {

inline constexpr int kSeparatorWidth = 26;
inline constexpr int kSeparatorHeight = 2;
inline constexpr int kSegmentGap = spacing::k8;

}  // namespace current_date

namespace welcome_message {

inline constexpr int kSectionGap = spacing::k2;
inline constexpr int kIconSize = icon::k44;
inline constexpr int kIconGap = spacing::k8;
inline constexpr int kIconYOffset = 6;

}  // namespace welcome_message

namespace current_progress {

inline constexpr int kSectionGap = spacing::k48;

}  // namespace current_progress

namespace global_footer {

inline constexpr int kBottomPadding = spacing::k16;
inline constexpr int kButtonSize = 58;
inline constexpr int kIconSize = icon::k44;
inline constexpr int kSdStatusYOffset = 1;

}  // namespace global_footer

namespace carousel {

inline constexpr int kPadding = spacing::k16;             // internal padding on all sides
inline constexpr int kDotDiameter = spacing::k20;         // 20x20 pagination dot
inline constexpr int kDotGap = spacing::k12;              // gap between dots
inline constexpr int kControlGap = spacing::k12;          // gap between Close / prev / next
inline constexpr int kContentControlGap = spacing::k16;   // gap between the slot and the control row
inline constexpr int kIconButtonSize = global_footer::kButtonSize;  // matches footer icon buttons
inline constexpr int kIconSize = global_footer::kIconSize;
// Touch-target inflation, mirroring the global footer (which shares this bottom y-band): generous
// vertically for low-drift taps, tight horizontally so adjacent controls tile without overlap.
inline constexpr int kTouchSlopY = spacing::k12;         // == footer's vertical hit slop
inline constexpr int kTouchSlopX = kControlGap / 2;      // half the gap: targets meet, no dead zone
inline constexpr uint8_t kActiveDotColor = color::kTextPrimary;      // filled black
inline constexpr uint8_t kInactiveDotColor = color::kGrayLight;      // light grey
// Greyed prev/next at the ends. kGrayDark (50% checkerboard dither) instead of kGrayLight (25%
// sparse) so the disabled chevron renders reliably on the first full refresh -- an isolated
// sparse-dither icon does not develop from a fresh white clear until a later partial refresh.
inline constexpr uint8_t kDisabledControlColor = color::kGrayDark;

}  // namespace carousel

// Full-page overlay that flips through Follow-up notes one "sticky" at a time. Borrows the vibe
// card's surface look and the modal's drop shadow; see epaper_ui/sticky_note.h.
namespace sticky_note {

inline constexpr int kScreenMargin = spacing::k20;  // 20px gap to every screen edge (below status bar)
inline constexpr int kCornerRadius = 4;             // matches the vibe card

}  // namespace sticky_note

namespace progress_bar {

inline constexpr int kBarHeight = spacing::k8;
inline constexpr int kMetaGap = spacing::k4;
inline constexpr int kBorderThickness = 1;

}  // namespace progress_bar

namespace sd_status {

inline constexpr int kMaxWidth = 220;
inline constexpr int kIconSize = icon::k44;
inline constexpr int kGap = spacing::k4;

}  // namespace sd_status

namespace button {

inline constexpr int kHeight = 56;
inline constexpr int kMinWidth = 144;
inline constexpr int kHorizontalPadding = spacing::k12;
inline constexpr int kLabelYOffset = 0;
inline constexpr int kBorderThickness = 2;
inline constexpr int kStrokeThickness = 2;

}  // namespace button

namespace button_icon {

inline constexpr int kSize = 44;
inline constexpr int kIconSize = icon::k36;
inline constexpr int kBorderThickness = 0;

}  // namespace button_icon

namespace mic_status {

inline constexpr int kSize = button_icon::kSize;
inline constexpr int kIconSize = button_icon::kIconSize;
inline constexpr int kBorderThickness = button_icon::kBorderThickness;
inline constexpr int kLabelGap = spacing::k4;
inline constexpr int kTrailingPadding = spacing::k12;

}  // namespace mic_status

namespace toggle {

inline constexpr int kWidth = 88;
inline constexpr int kHeight = 44;
inline constexpr int kThumbSize = icon::k36;
inline constexpr int kThumbInset = spacing::k4;
inline constexpr int kThumbBorderThickness = spacing::k4;
inline constexpr int kFocusRingThickness = 6;
inline constexpr int kFocusGap = spacing::k4;

}  // namespace toggle

namespace dashboard_page_menu {

inline constexpr int kUtilityItemCount = 0;
inline constexpr int kUtilityItemGap = spacing::k8;
inline constexpr int kMainItemCount = 5;

}  // namespace dashboard_page_menu

namespace badge {

inline constexpr int kHeight = 32;
inline constexpr int kMinWidth = 32;
inline constexpr int kBorderThickness = 2;
inline constexpr int kHorizontalPadding = spacing::k8;

}  // namespace badge

namespace completion_banner {

inline constexpr int kMinHeight = global_footer::kButtonSize + (2 * spacing::k8);
inline constexpr int kPadding = spacing::k8;
inline constexpr int kContentGap = spacing::k16;
inline constexpr int kIconContainerSize = global_footer::kButtonSize;
inline constexpr int kIconSize = icon::k44;
inline constexpr int kIconBorderThickness = spacing::k4;
inline constexpr int kLeftCornerRadius = global_footer::kButtonSize;
inline constexpr int kRightCornerRadius = spacing::k8;

}  // namespace completion_banner

namespace tag {

inline constexpr int kHorizontalPadding = spacing::k8;
inline constexpr int kVerticalPadding = spacing::k4;
inline constexpr int kBorderThickness = 0;

}  // namespace tag

namespace menu_item {

inline constexpr int kHeight = spacing::k72;
inline constexpr int kBottomBorderThickness = 1;
inline constexpr int kHorizontalPadding = spacing::k12;

}  // namespace menu_item

namespace menu_toggle {

inline constexpr int kHeight = spacing::k72;
inline constexpr int kBottomBorderThickness = 1;
inline constexpr int kHorizontalPadding = 0;
inline constexpr int kControlGap = spacing::k16;

}  // namespace menu_toggle

namespace network_item {

inline constexpr int kHeight = 48;
inline constexpr int kBottomBorderThickness = 1;
inline constexpr int kHorizontalPadding = spacing::k12;
inline constexpr int kIconSize = 36;
inline constexpr int kIconGap = spacing::k8;

}  // namespace network_item

namespace file_list_item {

inline constexpr int kHeight = 64;
inline constexpr int kHorizontalPadding = spacing::k12;
inline constexpr int kIconSlotSize = icon::k44;
inline constexpr int kIconGap = spacing::k12;
inline constexpr int kLabelGap = spacing::k12;
inline constexpr int kBottomBorderThickness = network_item::kBottomBorderThickness;

}  // namespace file_list_item

namespace file_list_footer {

inline constexpr int kHeight = 48;
inline constexpr int kBottomPadding = 0;
inline constexpr int kHorizontalPadding = spacing::k12;
inline constexpr int kIconSlotSize = icon::k36;
inline constexpr int kIconGap = spacing::k12;
inline constexpr int kLabelGap = spacing::k12;

}  // namespace file_list_footer

namespace file_list {

inline constexpr int kPanelBorderThickness = 1;
inline constexpr int kSectionGap = 0;
inline constexpr int kEmptyStateGap = spacing::k12;
inline constexpr int kEmptyStateIconSize = icon::k36;
inline constexpr int kScrollbarGap = spacing::k4;
inline constexpr int kScrollbarWidth = progress_bar::kBarHeight;
inline constexpr int kScrollbarBorderThickness = progress_bar::kBorderThickness;
inline constexpr int kFocusRingThickness = toggle::kFocusRingThickness;
inline constexpr int kFocusGap = toggle::kFocusGap;
inline constexpr int kMinThumbHeight = spacing::k16;

}  // namespace file_list

namespace list_item_header {

inline constexpr int kHeight = icon::k36;
inline constexpr int kIconSlotSize = icon::k36;
inline constexpr int kContentGap = spacing::k4;
inline constexpr int kDividerDotDiameter = spacing::k4;
inline constexpr int kTagIconSlotSize = icon::k36;
inline constexpr int kTagIconGap = spacing::k4;

}  // namespace list_item_header

namespace list_item {

inline constexpr int kPaddingLeft = spacing::k12;
inline constexpr int kPaddingRight = spacing::k12;
inline constexpr int kPaddingTop = spacing::k16;
inline constexpr int kPaddingBottom = spacing::k16;
inline constexpr int kRowGap = spacing::k2;
inline constexpr int kAccessoryGap = spacing::k8;
inline constexpr int kBottomBorderThickness = 0;
inline constexpr int kActionsGap = spacing::k12;
inline constexpr int kActionsHorizontalPadding = spacing::k12;
inline constexpr int kActionsBorderThickness = 1;
inline constexpr uint8_t kActionsBackgroundColor = color::kSurfaceRaised;
inline constexpr uint8_t kActionsBorderColor = color::kBorderStrong;

}  // namespace list_item

namespace vibe_card {

inline constexpr int kMinHeight = 220;
inline constexpr int kPadding = spacing::k16;
inline constexpr int kBorderThickness = 1;
inline constexpr int kTagHeaderGap = spacing::k12;
inline constexpr int kHeaderBodyGap = spacing::k4;
inline constexpr int kFooterGap = spacing::k32;
inline constexpr int kBodyLineGap = spacing::k4;
inline constexpr int kFooterIconSlotSize = icon::k44;
inline constexpr int kFooterButtonGap = spacing::k12;
inline constexpr int kBodyOutlineThickness = status_bar::kStrokeThickness;
inline constexpr uint8_t kBackgroundColor = color::kSurfaceBase;
inline constexpr uint8_t kEmptyStateBackgroundColor = color::kSurfaceRaised;
inline constexpr uint8_t kBorderColor = color::kBorderStrong;
inline constexpr uint8_t kFooterButtonBackgroundColor = color::kSurfaceEmphasis;

}  // namespace vibe_card

namespace timeline_list {

inline constexpr int kLabelHorizontalPadding = spacing::k8;
inline constexpr int kLabelVerticalPadding = spacing::k4;
inline constexpr int kLabelBorderThickness = spacing::k12;
inline constexpr int kLineIndent = spacing::k8;
inline constexpr int kItemIndent = spacing::k8;
inline constexpr int kLineThickness = 1;
inline constexpr int kEmptyStateMinHeight = spacing::k64 + spacing::k64 + spacing::k32;

}  // namespace timeline_list

namespace empty_state {

inline constexpr int kGap = spacing::k12;
inline constexpr int kIconSize = icon::k64;

}  // namespace empty_state

namespace time_input {

inline constexpr int kFieldHeight = button::kHeight;
inline constexpr int kHorizontalPadding = spacing::k12;
inline constexpr int kLabelGap = spacing::k4;
inline constexpr int kBorderThickness = 1;
inline constexpr int kSuffixGap = spacing::k12;

}  // namespace time_input

namespace network_list {

inline constexpr int kPanelBorderThickness = 1;
inline constexpr int kSectionGap = spacing::k12;
inline constexpr int kEmptyStateGap = empty_state::kGap;
inline constexpr int kEmptyStateIconSize = empty_state::kIconSize;

}  // namespace network_list

namespace scroll_container {

// 2px so the light-gray panel border stays legible on the e-paper panel.
inline constexpr int kPanelBorderThickness = 2;
inline constexpr int kFocusRingThickness = toggle::kFocusRingThickness;
inline constexpr int kFocusGap = toggle::kFocusGap;
inline constexpr int kContentPadding = spacing::k12;
inline constexpr int kScrollbarGap = kContentPadding;
inline constexpr int kScrollbarWidth = progress_bar::kBarHeight;
inline constexpr int kScrollbarBorderThickness = progress_bar::kBorderThickness;
inline constexpr int kLineGap = 0;
inline constexpr int kMinThumbHeight = spacing::k16;
inline constexpr int kEmptyStateGap = empty_state::kGap;
inline constexpr int kEmptyStateIconSize = empty_state::kIconSize;

}  // namespace scroll_container

namespace segment_control {

inline constexpr int kFieldHeight = button::kHeight;
inline constexpr int kInternalPadding = spacing::k8;
inline constexpr int kButtonHeight = kFieldHeight - (2 * kInternalPadding);
inline constexpr int kButtonGap = spacing::k8;
inline constexpr int kBorderThickness = 1;
inline constexpr int kFocusRingThickness = toggle::kFocusRingThickness;
inline constexpr int kFocusGap = toggle::kFocusGap;
inline constexpr int kButtonHorizontalPadding = spacing::k8;
inline constexpr int kButtonBorderThickness = 1;

}  // namespace segment_control

namespace password_input {

inline constexpr int kFieldHeight = button::kHeight;
inline constexpr int kBorderThickness = 1;
inline constexpr int kHorizontalPadding = spacing::k12;
inline constexpr int kIconSize = 36;
inline constexpr int kIconPadding = spacing::k4;
inline constexpr int kLabelGap = spacing::k4;

}  // namespace password_input

namespace number_input {

inline constexpr int kFieldHeight = button::kHeight;
inline constexpr int kBorderThickness = 1;
inline constexpr int kHorizontalPadding = spacing::k12;
inline constexpr int kLabelGap = spacing::k4;
inline constexpr int kButtonGap = spacing::k12;

}  // namespace number_input

namespace settings_page_menu {

inline constexpr int kItemCount = 5;

}  // namespace settings_page_menu

namespace modal {

inline constexpr int kScreenInset = spacing::k16;
inline constexpr int kHorizontalPadding = spacing::k14;
inline constexpr int kTopPadding = spacing::k14;
inline constexpr int kBottomPadding = spacing::k14;
inline constexpr int kTitleContentGap = spacing::k16;
inline constexpr int kContentActionGap = spacing::k24;
inline constexpr int kActionGap = spacing::k12;
inline constexpr int kBorderThickness = 2;
inline constexpr int kShadowOffset = spacing::k8;

}  // namespace modal

namespace toast {

inline constexpr int kScreenInset = spacing::k16;
inline constexpr int kBottomInset =
    global_footer::kBottomPadding + global_footer::kButtonSize + spacing::k16;
inline constexpr int kHorizontalPadding = spacing::k14;
inline constexpr int kVerticalPadding = spacing::k14;
inline constexpr int kIconSize = icon::k36;
inline constexpr int kIconGap = spacing::k8;
inline constexpr int kTextCloseGap = spacing::k12;
inline constexpr int kBorderThickness = 2;
inline constexpr int kShadowOffset = spacing::k8;

}  // namespace toast

}  // namespace design

#endif  // DESIGN_TOKENS_H_
