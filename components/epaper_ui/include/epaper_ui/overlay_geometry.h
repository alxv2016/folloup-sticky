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

}  // namespace epaper_ui

#endif  // EPAPER_UI_OVERLAY_GEOMETRY_H_
