#ifndef EPAPER_UI_LAYOUT_GRID_H_
#define EPAPER_UI_LAYOUT_GRID_H_

#include <cstdint>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

enum class FixedColumnSide : uint8_t {
    kNone,
    kLeft,
    kRight,
};

enum class FixedRowSide : uint8_t {
    kNone,
    kTop,
    kBottom,
};

struct LayoutGridStyle {
    int width = 0;
    int min_height = 0;
    int column_count = 1;
    int row_count = 1;
    int padding_left = design::spacing::k16;
    int padding_right = design::spacing::k16;
    int padding_top = 0;
    int padding_bottom = 0;
    int column_gap = design::spacing::k16;
    int row_gap = design::spacing::k16;
    FixedColumnSide fixed_column_side = FixedColumnSide::kNone;
    int fixed_column_width = 0;
    FixedRowSide fixed_row_side = FixedRowSide::kNone;
    int fixed_row_height = 0;
};

class LayoutGrid {
public:
    int origin_x = 0;
    int origin_y = 0;
    LayoutGridStyle style = {};

    UiRect Measure(int canvas_width) const;
    bool IsValid() const;
    UiRect ContentBounds(int canvas_width) const;
    UiRect CellBounds(int canvas_width,
                      int column,
                      int row,
                      int column_span = 1,
                      int row_span = 1) const;
    int ColumnWidth(int canvas_width, int column) const;
    int RowHeight(int canvas_width, int row) const;
};

}  // namespace epaper_ui

#endif  // EPAPER_UI_LAYOUT_GRID_H_
