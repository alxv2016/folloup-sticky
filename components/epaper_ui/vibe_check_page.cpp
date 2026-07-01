#include "epaper_ui/vibe_check_page.h"

#include <algorithm>
#include <string>
#include <vector>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kMargin = design::spacing::k16;
constexpr int kContentTopGap = design::spacing::k16;
constexpr int kHeadingCardGap = design::spacing::k24;
constexpr int kCardProgressGap = design::spacing::k32;
constexpr int kProgressMessageGap = design::spacing::k24;
constexpr int kMessageFooterGap = design::spacing::k16;
constexpr auto kHeadingRole = design::TypographyRole::kHeadingH1;
constexpr auto kMessageRole = design::TypographyRole::kLabelSmall;

int PageWidth(int portrait_width)
{
    return std::max(0, portrait_width - (2 * kMargin));
}

// Top of the global footer, computed the same way the footer positions itself.
int FooterTop(int portrait_height)
{
    return portrait_height - design::global_footer::kBottomPadding -
           design::global_footer::kButtonSize;
}

VibeCardStyle CardStyle(int width, int height)
{
    VibeCardStyle style = {};
    style.width = width;
    style.height = height;
    return style;
}

ProgressBarStyle ProgressStyle(int width)
{
    ProgressBarStyle style = {};
    style.width = width;
    return style;
}

int MessageHeight(const std::string& text, int width)
{
    if (text.empty()) {
        return 0;
    }
    return static_cast<int>(WrapTextToWidth(kMessageRole, text, width).size()) *
           LineHeight(kMessageRole);
}

struct Layout {
    UiRect heading = {};
    UiRect card = {};
    UiRect progress = {};
    int message_y = 0;
    int card_height = 0;
};

Layout BuildLayout(int portrait_width, int portrait_height, const VibeCheckPageState& state)
{
    const int page_width = PageWidth(portrait_width);
    const int content_top = StatusBarHeight() + kContentTopGap;

    Layout layout = {};
    layout.heading = {kMargin, content_top, page_width, LineHeight(kHeadingRole)};

    // Progress + message form a block anchored above the footer; the message sits closest to
    // it, with a 16px gap between the message and the footer.
    const int footer_top = FooterTop(portrait_height);
    const int message_height = MessageHeight(state.message_text, page_width);
    layout.message_y = footer_top - kMessageFooterGap - message_height;

    const int progress_height =
        ProgressBarBounds(kMargin, 0, state.progress, ProgressStyle(page_width)).height;
    const int progress_y = layout.message_y - kProgressMessageGap - progress_height;
    layout.progress = {kMargin, progress_y, page_width, progress_height};

    // The card stretches to fill everything between the heading and that bottom block.
    const int card_top = layout.heading.bottom() + kHeadingCardGap;
    layout.card_height = std::max(0, progress_y - kCardProgressGap - card_top);
    layout.card = {kMargin, card_top, page_width, layout.card_height};
    return layout;
}

}  // namespace

UiRect VibeCheckCardBounds(int portrait_width,
                           int portrait_height,
                           const VibeCheckPageState& state)
{
    return BuildLayout(portrait_width, portrait_height, state).card;
}

bool HitTestVibeCheckCard(int portrait_width,
                          int portrait_height,
                          const VibeCheckPageState& state,
                          int x,
                          int y)
{
    const UiRect card = VibeCheckCardBounds(portrait_width, portrait_height, state);
    return !card.IsEmpty() && card.Contains(x, y);
}

bool HitTestVibeCheckAction(int portrait_width,
                            int portrait_height,
                            const VibeCheckPageState& state,
                            int x,
                            int y,
                            VibeCardActionSelection* action)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    return HitTestVibeCardAction(layout.card.x, layout.card.y, portrait_width, state.card,
                                 CardStyle(PageWidth(portrait_width), layout.card_height), x, y,
                                 action);
}

void DrawVibeCheckPage(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const VibeCheckPageState& state,
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

    DrawVibeCard(framebuffer, raw_width, raw_height, portrait_width, portrait_height, layout.card.x,
                 layout.card.y, state.card, CardStyle(page_width, layout.card_height));

    DrawProgressBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                    layout.progress.x, layout.progress.y, state.progress, ProgressStyle(page_width));

    if (!state.message_text.empty()) {
        const std::vector<std::string> lines =
            WrapTextToWidth(kMessageRole, state.message_text, page_width);
        const int line_height = LineHeight(kMessageRole);
        int cursor_y = layout.message_y;
        for (const std::string& line : lines) {
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               kMargin, cursor_y, line, kMessageRole, design::color::kBlack);
            cursor_y += line_height;
        }
    }

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
