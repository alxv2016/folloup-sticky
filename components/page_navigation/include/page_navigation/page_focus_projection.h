#ifndef PAGE_NAVIGATION_PAGE_FOCUS_PROJECTION_H_
#define PAGE_NAVIGATION_PAGE_FOCUS_PROJECTION_H_

#include "page_navigation/navigation_model.h"

namespace page_navigation {

struct PageFocusProjection {
    int navigation_focus_index = -1;
    int page_menu_selected_index = -1;
    int footer_selected_index = -1;
};

int ResolveNavigationFocusIndex(const NavigationModel& model,
                                NavigationItemSection page_menu_section,
                                int page_menu_selected_index,
                                int footer_selected_index);

int PageMenuSelectedIndexForFocus(const NavigationModel& model,
                                  int navigation_focus_index,
                                  NavigationItemSection page_menu_section,
                                  int no_selection_index);

int FooterSelectedIndexForFocus(const NavigationModel& model,
                                int navigation_focus_index,
                                int no_selection_index);

PageFocusProjection ProjectPageFocus(const NavigationModel& model,
                                     NavigationItemSection page_menu_section,
                                     int navigation_focus_index,
                                     int page_menu_no_selection_index,
                                     int footer_no_selection_index);

PageFocusProjection ResolvePageFocusProjection(const NavigationModel& model,
                                               NavigationItemSection page_menu_section,
                                               int navigation_focus_index,
                                               int page_menu_selected_index,
                                               int footer_selected_index,
                                               int page_menu_no_selection_index,
                                               int footer_no_selection_index);

}  // namespace page_navigation

#endif  // PAGE_NAVIGATION_PAGE_FOCUS_PROJECTION_H_
