#include "epaper_ui/sd_status.h"

#include <algorithm>
#include <string_view>

#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr std::string_view kStorageLabel = "Ext storage";
constexpr std::string_view kNoSdCardStatus = "No SD card";

const EmbeddedImageAsset* ResolveSdIcon()
{
    return project_assets::GetIcon(EmbeddedIconId::kSdCard);
}

void DrawScaledMonoAsset(uint8_t* framebuffer,
                         int raw_width,
                         int raw_height,
                         int portrait_width,
                         int portrait_height,
                         int origin_x,
                         int origin_y,
                         const EmbeddedImageAsset* asset,
                         int target_size,
                         uint8_t tone)
{
    if (asset == nullptr || asset->data == nullptr || target_size <= 0) {
        return;
    }

    for (int y = 0; y < target_size; ++y) {
        const int source_y = std::min(asset->height - 1, (y * asset->height) / target_size);
        for (int x = 0; x < target_size; ++x) {
            const int source_x = std::min(asset->width - 1, (x * asset->width) / target_size);
            const size_t byte_index =
                static_cast<size_t>(source_y) * asset->stride_bytes +
                static_cast<size_t>(source_x / 8);
            const uint8_t bit_mask = static_cast<uint8_t>(0x80U >> (source_x & 0x07));
            if ((asset->data[byte_index] & bit_mask) == 0) {
                continue;
            }

            const int px = origin_x + x;
            const int py = origin_y + y;
            DrawPortraitPixel(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              px,
                              py,
                              ShouldDrawBlackForTone(px, py, tone));
        }
    }
}

std::string ResolveStatusText(const SdStatusState& state)
{
    return state.has_sd_card ? state.free_space_text : std::string(kNoSdCardStatus);
}

}  // namespace

UiRect SdStatusBounds(int origin_x, int origin_y, const SdStatusState& state, const SdStatusStyle& style)
{
    const int icon_size = ClampPositive(style.icon_size);
    const int gap = ClampPositive(style.gap);
    const int width = ClampPositive(style.max_width);

    ProgressBarStyle progress_style = style.progress;
    progress_style.width = std::max(0, width - icon_size - gap);
    const ProgressBarState progress_state = {
        .label_text = std::string(kStorageLabel),
        .status_text = ResolveStatusText(state),
        .progress_percent = state.has_sd_card ? std::clamp(state.used_percent, 0, 100) : 0,
    };
    const UiRect progress_bounds =
        ProgressBarBounds(origin_x + icon_size + gap, origin_y, progress_state, progress_style);
    return {origin_x, origin_y, width, std::max(icon_size, progress_bounds.height)};
}

void DrawSdStatus(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const SdStatusState& state,
                  const SdStatusStyle& style)
{
    const UiRect bounds = SdStatusBounds(origin_x, origin_y, state, style);
    if (bounds.IsEmpty()) {
        return;
    }

    const int icon_size = ClampPositive(style.icon_size);
    const int gap = ClampPositive(style.gap);
    const EmbeddedImageAsset* icon_asset = ResolveSdIcon();

    ProgressBarStyle progress_style = style.progress;
    progress_style.width = std::max(0, bounds.width - icon_size - gap);
    progress_style.label_role = design::TypographyRole::kLabelSmallBlack;
    const ProgressBarState progress_state = {
        .label_text = std::string(kStorageLabel),
        .status_text = ResolveStatusText(state),
        .progress_percent = state.has_sd_card ? std::clamp(state.used_percent, 0, 100) : 0,
    };
    const UiRect progress_bounds = ProgressBarBounds(
        origin_x + icon_size + gap, origin_y, progress_state, progress_style);

    const int icon_y = origin_y + std::max(0, (bounds.height - icon_size) / 2);
    const int progress_y = origin_y + std::max(0, (bounds.height - progress_bounds.height) / 2);
    if (icon_asset != nullptr) {
        DrawScaledMonoAsset(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            origin_x,
                            icon_y,
                            icon_asset,
                            icon_size,
                            design::color::kBlack);
    }

    DrawProgressBar(framebuffer,
                    raw_width,
                    raw_height,
                    portrait_width,
                    portrait_height,
                    origin_x + icon_size + gap,
                    progress_y,
                    progress_state,
                    progress_style);
}

}  // namespace epaper_ui
