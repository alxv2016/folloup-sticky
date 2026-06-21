#include "page_navigation/roving_focus.h"

namespace page_navigation {

RovingFocus::RovingFocus(int item_count, int initial_index)
{
    Configure(item_count, initial_index);
}

void RovingFocus::Configure(int item_count, int initial_index)
{
    item_count_ = item_count > 0 ? item_count : 0;
    index_ = item_count_ > 0 ? WrapIndex(initial_index, item_count_) : -1;
}

bool RovingFocus::Move(int delta)
{
    if (item_count_ <= 0 || delta == 0) {
        return false;
    }

    const int previous_index = index_;
    const int base_index = previous_index >= 0 ? previous_index : 0;
    index_ = WrapIndex(base_index + delta, item_count_);
    return index_ != previous_index;
}

bool RovingFocus::SetIndex(int index)
{
    if (item_count_ <= 0) {
        return false;
    }

    const int wrapped_index = WrapIndex(index, item_count_);
    if (wrapped_index == index_) {
        return false;
    }

    index_ = wrapped_index;
    return true;
}

int RovingFocus::WrapIndex(int index, int item_count)
{
    if (item_count <= 0) {
        return -1;
    }

    return ((index % item_count) + item_count) % item_count;
}

}  // namespace page_navigation
