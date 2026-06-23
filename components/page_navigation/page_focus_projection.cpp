#include "page_navigation/page_focus_projection.h"

namespace page_navigation {

int ResolveNavigationFocusIndex(const NavigationModel& model,
                                NavigationItemSection page_menu_section,
                                int page_menu_selected_index,
                                int footer_selected_index)
{
    for (int index = 0; index < model.item_count; ++index) {
        const NavigationItemDescriptor* item = model.ItemAt(index);
        if (item == nullptr) {
            continue;
        }
        if (item->section == page_menu_section &&
            page_menu_selected_index == item->item_index) {
            return index;
        }
        if (item->section == NavigationItemSection::kFooter &&
            footer_selected_index == item->item_index) {
            return index;
        }
    }
    return -1;
}

int PageMenuSelectedIndexForFocus(const NavigationModel& model,
                                  int navigation_focus_index,
                                  NavigationItemSection page_menu_section,
                                  int no_selection_index)
{
    const NavigationItemDescriptor* item = model.ItemAt(navigation_focus_index);
    return item != nullptr && item->section == page_menu_section ? item->item_index
                                                                 : no_selection_index;
}

int FooterSelectedIndexForFocus(const NavigationModel& model,
                                int navigation_focus_index,
                                int no_selection_index)
{
    const NavigationItemDescriptor* item = model.ItemAt(navigation_focus_index);
    return item != nullptr && item->section == NavigationItemSection::kFooter ? item->item_index
                                                                              : no_selection_index;
}

PageFocusProjection ProjectPageFocus(const NavigationModel& model,
                                     NavigationItemSection page_menu_section,
                                     int navigation_focus_index,
                                     int page_menu_no_selection_index,
                                     int footer_no_selection_index)
{
    return {
        .navigation_focus_index = navigation_focus_index,
        .page_menu_selected_index =
            PageMenuSelectedIndexForFocus(model,
                                          navigation_focus_index,
                                          page_menu_section,
                                          page_menu_no_selection_index),
        .footer_selected_index =
            FooterSelectedIndexForFocus(model,
                                        navigation_focus_index,
                                        footer_no_selection_index),
    };
}

PageFocusProjection ResolvePageFocusProjection(const NavigationModel& model,
                                               NavigationItemSection page_menu_section,
                                               int navigation_focus_index,
                                               int page_menu_selected_index,
                                               int footer_selected_index,
                                               int page_menu_no_selection_index,
                                               int footer_no_selection_index)
{
    const int resolved_focus_index =
        model.ItemAt(navigation_focus_index) != nullptr
            ? navigation_focus_index
            : ResolveNavigationFocusIndex(model,
                                          page_menu_section,
                                          page_menu_selected_index,
                                          footer_selected_index);
    return ProjectPageFocus(model,
                            page_menu_section,
                            resolved_focus_index,
                            page_menu_no_selection_index,
                            footer_no_selection_index);
}

}  // namespace page_navigation
