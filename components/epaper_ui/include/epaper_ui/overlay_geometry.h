#ifndef EPAPER_UI_OVERLAY_GEOMETRY_H_
#define EPAPER_UI_OVERLAY_GEOMETRY_H_

namespace epaper_ui {

struct UiRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool IsEmpty() const
    {
        return width <= 0 || height <= 0;
    }

    int right() const
    {
        return x + width;
    }

    int bottom() const
    {
        return y + height;
    }

    bool Contains(int px, int py) const
    {
        return px >= x && py >= y && px < right() && py < bottom();
    }
};

inline UiRect UnionRect(const UiRect& lhs, const UiRect& rhs)
{
    if (lhs.IsEmpty()) {
        return rhs;
    }
    if (rhs.IsEmpty()) {
        return lhs;
    }

    const int x0 = lhs.x < rhs.x ? lhs.x : rhs.x;
    const int y0 = lhs.y < rhs.y ? lhs.y : rhs.y;
    const int x1 = lhs.right() > rhs.right() ? lhs.right() : rhs.right();
    const int y1 = lhs.bottom() > rhs.bottom() ? lhs.bottom() : rhs.bottom();
    return {x0, y0, x1 - x0, y1 - y0};
}

}  // namespace epaper_ui

#endif  // EPAPER_UI_OVERLAY_GEOMETRY_H_
