#include "epaper_ui/todos_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kMargin = design::spacing::k16;
constexpr int kContentTopGap = design::spacing::k16;
constexpr int kHeadingTimelineGap = design::spacing::k24;
constexpr int kTimelineFooterGap = design::spacing::k16;
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

struct Layout {
    UiRect heading = {};
    UiRect timeline = {};
};

Layout BuildLayout(int portrait_width, int portrait_height)
{
    const int page_width = PageWidth(portrait_width);
    const int content_top = StatusBarHeight() + kContentTopGap;

    Layout layout = {};
    layout.heading = {kMargin, content_top, page_width, LineHeight(kHeadingRole)};

    const int timeline_top = layout.heading.bottom() + kHeadingTimelineGap;
    const int footer_top = FooterTop(portrait_height);
    const int timeline_height = std::max(0, footer_top - kTimelineFooterGap - timeline_top);
    layout.timeline = {kMargin, timeline_top, page_width, timeline_height};
    return layout;
}

TimelineListStyle TimelineStyle(const UiRect& timeline)
{
    TimelineListStyle style = {};
    style.width = timeline.width;
    style.height = timeline.height;
    return style;
}

}  // namespace

TodosTimelineHit HitTestTodosTimeline(int portrait_width,
                                      int portrait_height,
                                      const TodosPageState& state,
                                      int x,
                                      int y)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height);
    if (layout.timeline.IsEmpty() || !layout.timeline.Contains(x, y)) {
        return {};
    }
    const TimelineLayout timeline_layout =
        BuildTimelineLayout(portrait_width, portrait_height, layout.timeline.x, layout.timeline.y,
                            state.timeline, TimelineStyle(layout.timeline));
    for (const TimelineGroupHit& group : timeline_layout.groups) {
        for (const TimelineItemHit& item : group.items) {
            if (!item.bounds.IsEmpty() && item.bounds.Contains(x, y)) {
                return {NotesTimelineHitKind::kItem, group.group_index, item.item_index};
            }
        }
    }
    for (const TimelineGroupHit& group : timeline_layout.groups) {
        if (!group.chip_bounds.IsEmpty() && group.chip_bounds.Contains(x, y)) {
            return {NotesTimelineHitKind::kGroupChip, group.group_index, -1};
        }
    }
    return {};
}

void DrawTodosPage(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const TodosPageState& state,
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

    const Layout layout = BuildLayout(portrait_width, portrait_height);

    if (!state.title_text.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.heading.x, layout.heading.y, state.title_text, kHeadingRole,
                           design::color::kBlack);
    }

    DrawTimelineList(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     layout.timeline.x, layout.timeline.y, state.timeline,
                     TimelineStyle(layout.timeline));

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
