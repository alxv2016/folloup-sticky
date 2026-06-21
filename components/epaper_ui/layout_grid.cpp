#include "epaper_ui/layout_grid.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

int ResolveWidth(int canvas_width, int origin_x, const LayoutGridStyle& style)
{
    if (style.width > 0) {
        return style.width;
    }
    return std::max(0, canvas_width - origin_x);
}

int ResolveColumnGap(const LayoutGridStyle& style)
{
    return style.column_count > 1 ? ClampPositive(style.column_gap) : 0;
}

int ResolveRowGap(const LayoutGridStyle& style)
{
    return style.row_count > 1 ? ClampPositive(style.row_gap) : 0;
}

int ResolveAvailableColumnWidth(const UiRect& content, const LayoutGridStyle& style)
{
    const int total_gap = ResolveColumnGap(style) * std::max(0, style.column_count - 1);
    return std::max(0, content.width - total_gap);
}

int ResolveAvailableRowHeight(const UiRect& content, const LayoutGridStyle& style)
{
    const int total_gap = ResolveRowGap(style) * std::max(0, style.row_count - 1);
    return std::max(0, content.height - total_gap);
}

bool HasFixedColumn(const LayoutGridStyle& style)
{
    return style.column_count > 1 && style.fixed_column_width > 0 &&
           style.fixed_column_side != FixedColumnSide::kNone;
}

bool HasFixedRow(const LayoutGridStyle& style)
{
    return style.row_count > 1 && style.fixed_row_height > 0 &&
           style.fixed_row_side != FixedRowSide::kNone;
}

int ColumnWidthForIndex(const UiRect& content, const LayoutGridStyle& style, int column)
{
    if (style.column_count <= 0 || column < 0 || column >= style.column_count) {
        return 0;
    }

    const int available_width = ResolveAvailableColumnWidth(content, style);
    if (available_width <= 0) {
        return 0;
    }

    if (!HasFixedColumn(style)) {
        const int base_width = available_width / style.column_count;
        const int remainder = available_width % style.column_count;
        return base_width + (column < remainder ? 1 : 0);
    }

    const int fixed_index =
        style.fixed_column_side == FixedColumnSide::kLeft ? 0 : style.column_count - 1;
    const int fixed_width = std::min(available_width, ClampPositive(style.fixed_column_width));
    if (column == fixed_index) {
        return fixed_width;
    }

    const int grow_count = style.column_count - 1;
    if (grow_count <= 0) {
        return available_width;
    }

    const int grow_width = std::max(0, available_width - fixed_width);
    const int base_width = grow_width / grow_count;
    const int remainder = grow_width % grow_count;
    const int grow_index = column < fixed_index ? column : column - 1;
    return base_width + (grow_index < remainder ? 1 : 0);
}

int RowHeightForIndex(const UiRect& content, const LayoutGridStyle& style, int row)
{
    if (style.row_count <= 0 || row < 0 || row >= style.row_count) {
        return 0;
    }

    const int available_height = ResolveAvailableRowHeight(content, style);
    if (available_height <= 0) {
        return 0;
    }

    if (!HasFixedRow(style)) {
        const int base_height = available_height / style.row_count;
        const int remainder = available_height % style.row_count;
        return base_height + (row < remainder ? 1 : 0);
    }

    const int fixed_index = style.fixed_row_side == FixedRowSide::kTop ? 0 : style.row_count - 1;
    const int fixed_height = std::min(available_height, ClampPositive(style.fixed_row_height));
    if (row == fixed_index) {
        return fixed_height;
    }

    const int grow_count = style.row_count - 1;
    if (grow_count <= 0) {
        return available_height;
    }

    const int grow_height = std::max(0, available_height - fixed_height);
    const int base_height = grow_height / grow_count;
    const int remainder = grow_height % grow_count;
    const int grow_index = row < fixed_index ? row : row - 1;
    return base_height + (grow_index < remainder ? 1 : 0);
}

int ColumnStartForIndex(const UiRect& content, const LayoutGridStyle& style, int column)
{
    if (column <= 0) {
        return content.x;
    }

    const int gap = ResolveColumnGap(style);
    int x = content.x;
    for (int index = 0; index < column; ++index) {
        x += ColumnWidthForIndex(content, style, index);
        if (index + 1 < style.column_count) {
            x += gap;
        }
    }
    return x;
}

int RowStartForIndex(const UiRect& content, const LayoutGridStyle& style, int row)
{
    if (row <= 0) {
        return content.y;
    }

    const int gap = ResolveRowGap(style);
    int y = content.y;
    for (int index = 0; index < row; ++index) {
        y += RowHeightForIndex(content, style, index);
        if (index + 1 < style.row_count) {
            y += gap;
        }
    }
    return y;
}

}  // namespace

UiRect LayoutGrid::Measure(int canvas_width) const
{
    const int width = ResolveWidth(canvas_width, origin_x, style);
    const int height = std::max(
        ClampPositive(style.min_height),
        ClampPositive(style.padding_top) + ClampPositive(style.padding_bottom) +
            ResolveRowGap(style) * std::max(0, style.row_count - 1));
    return {origin_x, origin_y, width, height};
}

bool LayoutGrid::IsValid() const
{
    return style.column_count > 0 && style.row_count > 0;
}

UiRect LayoutGrid::ContentBounds(int canvas_width) const
{
    const UiRect bounds = Measure(canvas_width);
    const int left = ClampPositive(style.padding_left);
    const int right = ClampPositive(style.padding_right);
    const int top = ClampPositive(style.padding_top);
    const int bottom = ClampPositive(style.padding_bottom);
    return {bounds.x + left,
            bounds.y + top,
            std::max(0, bounds.width - left - right),
            std::max(0, bounds.height - top - bottom)};
}

UiRect LayoutGrid::CellBounds(int canvas_width,
                              int column,
                              int row,
                              int column_span,
                              int row_span) const
{
    if (!IsValid() || column < 0 || row < 0 || column >= style.column_count ||
        row >= style.row_count) {
        return {origin_x, origin_y, 0, 0};
    }

    const int clamped_column_span = std::clamp(column_span, 1, style.column_count - column);
    const int clamped_row_span = std::clamp(row_span, 1, style.row_count - row);
    const UiRect content = ContentBounds(canvas_width);
    const int column_gap = ResolveColumnGap(style);
    const int row_gap = ResolveRowGap(style);

    int width = 0;
    for (int index = 0; index < clamped_column_span; ++index) {
        width += ColumnWidthForIndex(content, style, column + index);
    }
    if (clamped_column_span > 1) {
        width += column_gap * (clamped_column_span - 1);
    }

    int height = 0;
    for (int index = 0; index < clamped_row_span; ++index) {
        height += RowHeightForIndex(content, style, row + index);
    }
    if (clamped_row_span > 1) {
        height += row_gap * (clamped_row_span - 1);
    }

    return {ColumnStartForIndex(content, style, column),
            RowStartForIndex(content, style, row),
            width,
            height};
}

int LayoutGrid::ColumnWidth(int canvas_width, int column) const
{
    return ColumnWidthForIndex(ContentBounds(canvas_width), style, column);
}

int LayoutGrid::RowHeight(int canvas_width, int row) const
{
    return RowHeightForIndex(ContentBounds(canvas_width), style, row);
}

}  // namespace epaper_ui
