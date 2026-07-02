#include "epaper_ui/summarize_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kMargin = design::spacing::k16;
constexpr int kContentTopGap = design::spacing::k16;
constexpr int kHeadingSegmentGap = design::spacing::k24;
constexpr int kSegmentScrollGap = design::spacing::k24;
constexpr int kScrollButtonGap = design::spacing::k16;
constexpr int kButtonFooterGap = design::spacing::k16;
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

SegmentControlStyle SegmentStyle(int width)
{
    SegmentControlStyle style = {};
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

ButtonStyle GetSummaryButtonStyle(int width)
{
    ButtonStyle style = {};
    style.width = width;
    // Sole action on the page -> primary (darker) variant.
    style.variant = ButtonVariant::kPrimary;
    return style;
}

struct Layout {
    UiRect heading = {};
    UiRect segment = {};
    UiRect scroll = {};
    UiRect button = {};
    int scroll_panel_height = 0;
};

Layout BuildLayout(int portrait_width, int portrait_height, const SummarizePageState& state)
{
    const int page_width = PageWidth(portrait_width);
    const int content_top = StatusBarHeight() + kContentTopGap;

    Layout layout = {};
    layout.heading = {kMargin, content_top, page_width, LineHeight(kHeadingRole)};

    const int segment_top = layout.heading.bottom() + kHeadingSegmentGap;
    layout.segment = SegmentControlBounds(kMargin, segment_top, SegmentStyle(page_width));

    const int scroll_top = layout.segment.bottom() + kSegmentScrollGap;
    const int footer_top = FooterTop(portrait_height);
    const int button_height = design::button::kHeight;
    const int button_top = std::max(scroll_top, footer_top - kButtonFooterGap - button_height);

    layout.scroll_panel_height = std::max(0, button_top - scroll_top - kScrollButtonGap);
    layout.scroll = {kMargin, scroll_top, page_width, layout.scroll_panel_height};
    layout.button = {kMargin, button_top, page_width, button_height};
    return layout;
}

}  // namespace

bool HitTestSummarizeSegment(int portrait_width,
                             int portrait_height,
                             const SummarizePageState& state,
                             int x,
                             int y,
                             int* segment_index)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    return HitTestSegmentControlSegment(layout.segment.x, layout.segment.y, state.segment_control,
                                        SegmentStyle(PageWidth(portrait_width)), x, y,
                                        segment_index);
}

bool HitTestSummarizeScrollContainer(int portrait_width,
                                     int portrait_height,
                                     const SummarizePageState& state,
                                     int x,
                                     int y)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    return !layout.scroll.IsEmpty() && layout.scroll.Contains(x, y);
}

bool HitTestSummarizeGetSummaryButton(int portrait_width,
                                      int portrait_height,
                                      const SummarizePageState& state,
                                      int x,
                                      int y)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    return !layout.button.IsEmpty() && layout.button.Contains(x, y);
}

void DrawSummarizePage(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const SummarizePageState& state,
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
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);

    if (!state.title_text.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.heading.x, layout.heading.y, state.title_text, kHeadingRole,
                           design::color::kBlack);
    }

    DrawSegmentControl(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       layout.segment.x, layout.segment.y, state.segment_control,
                       SegmentStyle(page_width));

    DrawScrollContainer(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                        layout.scroll.x, layout.scroll.y, state.scroll_container,
                        ScrollStyle(page_width, layout.scroll_panel_height));

    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height, layout.button.x,
               layout.button.y, state.get_summary_button, GetSummaryButtonStyle(page_width));

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
