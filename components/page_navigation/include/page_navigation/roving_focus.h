#ifndef PAGE_NAVIGATION_ROVING_FOCUS_H_
#define PAGE_NAVIGATION_ROVING_FOCUS_H_

namespace page_navigation {

// RovingFocus is the source of truth for a focusable scope's active index.
// It owns wraparound index movement only; it does not own input routing,
// rendering, modal visibility, or display refresh behavior.
class RovingFocus {
public:
    RovingFocus(int item_count = 0, int initial_index = 0);

    void Configure(int item_count, int initial_index = 0);
    bool Move(int delta);
    bool SetIndex(int index);

    int index() const { return index_; }
    int item_count() const { return item_count_; }

    static int WrapIndex(int index, int item_count);

private:
    int item_count_ = 0;
    int index_ = -1;
};

}  // namespace page_navigation

#endif  // PAGE_NAVIGATION_ROVING_FOCUS_H_
