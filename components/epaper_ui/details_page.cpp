#include "epaper_ui/details_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kMargin = design::spacing::k16;
constexpr int kContentTopGap = design::spacing::k16;
constexpr int kHeadingHeaderGap = design::spacing::k16;
constexpr int kHeaderScrollGap = design::spacing::k16;
constexpr int kScrollButtonGap = design::spacing::k16;
constexpr int kButtonFooterGap = design::spacing::k16;
constexpr int kButtonGap = design::spacing::k16;
constexpr auto kHeadingRole = design::TypographyRole::kHeadingH1;

int PageWidth(int portrait_width)
{
    return std::max(0, portrait_width - (2 * kMargin));
}

int FooterTop(int portrait_height)
{
    return portrait_height - design::global_footer::kBottomPadding -
           design::global_footer::kButtonSize;
}

ListItemHeaderStyle HeaderStyle(int width)
{
    ListItemHeaderStyle style = {};
    style.width = width;
    return style;
}

ScrollContainerStyle ScrollStyle(int width, int panel_height)
{
    ScrollContainerStyle style = {};
    style.width = width;
    style.panel_height = panel_height;
    return style;
}

ButtonStyle MakeButtonStyle(int width, ButtonVariant variant)
{
    ButtonStyle style = {};
    style.width = width;
    style.variant = variant;
    return style;
}

struct Layout {
    UiRect heading = {};
    UiRect header = {};
    UiRect scroll = {};
    UiRect back_button = {};
    UiRect transcribe_button = {};
    int scroll_panel_height = 0;
};

Layout BuildLayout(int portrait_width, int portrait_height, bool show_transcribe)
{
    const int page_width = PageWidth(portrait_width);
    const int content_top = StatusBarHeight() + kContentTopGap;

    Layout layout = {};
    layout.heading = {kMargin, content_top, page_width, LineHeight(kHeadingRole)};
    layout.header = {kMargin, layout.heading.bottom() + kHeadingHeaderGap, page_width,
                     design::list_item_header::kHeight};

    const int scroll_top = layout.header.bottom() + kHeaderScrollGap;
    const int footer_top = FooterTop(portrait_height);
    const int button_height = design::button::kHeight;
    const int button_top = std::max(scroll_top, footer_top - kButtonFooterGap - button_height);
    layout.scroll_panel_height = std::max(0, button_top - scroll_top - kScrollButtonGap);
    layout.scroll = {kMargin, scroll_top, page_width, layout.scroll_panel_height};
    if (show_transcribe) {
        // Back (secondary) on the left, Transcribe (primary) on the right, split evenly.
        const int half_width = std::max(0, (page_width - kButtonGap) / 2);
        layout.back_button = {kMargin, button_top, half_width, button_height};
        const int right_x = kMargin + half_width + kButtonGap;
        layout.transcribe_button = {right_x, button_top,
                                    std::max(0, portrait_width - kMargin - right_x), button_height};
    } else {
        layout.back_button = {kMargin, button_top, page_width, button_height};
    }
    return layout;
}

}  // namespace

bool HitTestDetailsScrollContainer(int portrait_width,
                                   int portrait_height,
                                   const DetailsPageState& state,
                                   int x,
                                   int y)
{
    const Layout layout =
        BuildLayout(portrait_width, portrait_height, state.show_transcribe_button);
    return !layout.scroll.IsEmpty() && layout.scroll.Contains(x, y);
}

bool HitTestDetailsBackButton(int portrait_width,
                              int portrait_height,
                              const DetailsPageState& state,
                              int x,
                              int y)
{
    const Layout layout =
        BuildLayout(portrait_width, portrait_height, state.show_transcribe_button);
    return !layout.back_button.IsEmpty() && layout.back_button.Contains(x, y);
}

bool HitTestDetailsTranscribeButton(int portrait_width,
                                    int portrait_height,
                                    const DetailsPageState& state,
                                    int x,
                                    int y)
{
    if (!state.show_transcribe_button) {
        return false;
    }
    const Layout layout =
        BuildLayout(portrait_width, portrait_height, state.show_transcribe_button);
    return !layout.transcribe_button.IsEmpty() && layout.transcribe_button.Contains(x, y);
}

void DrawDetailsPage(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     const DetailsPageState& state,
                     const StatusBarState& status_bar_state,
                     const GlobalFooterState& footer_state)
{
    if (framebuffer == nullptr) {
        return;
    }

    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     {0, 0, portrait_width, portrait_height}, design::color::kWhite);
    DrawStatusBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  status_bar_state);

    const int page_width = PageWidth(portrait_width);
    const Layout layout =
        BuildLayout(portrait_width, portrait_height, state.show_transcribe_button);

    if (!state.title_text.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.heading.x, layout.heading.y, state.title_text, kHeadingRole,
                           design::color::kBlack);
    }

    DrawListItemHeader(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       layout.header.x, layout.header.y, state.recording_header,
                       HeaderStyle(page_width));

    DrawScrollContainer(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                        layout.scroll.x, layout.scroll.y, state.scroll_container,
                        ScrollStyle(page_width, layout.scroll_panel_height));

    // When Transcribe is shown, Back drops to the secondary (kDefault) variant; alone it stays the
    // sole primary action.
    const ButtonVariant back_variant =
        state.show_transcribe_button ? ButtonVariant::kDefault : ButtonVariant::kPrimary;
    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
               layout.back_button.x, layout.back_button.y, state.back_button,
               MakeButtonStyle(layout.back_button.width, back_variant));
    if (state.show_transcribe_button) {
        DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                   layout.transcribe_button.x, layout.transcribe_button.y, state.transcribe_button,
                   MakeButtonStyle(layout.transcribe_button.width, ButtonVariant::kPrimary));
    }

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
