#ifndef PROJECT_ASSET_TYPES_H
#define PROJECT_ASSET_TYPES_H

#include <cstddef>
#include <cstdint>

enum class ImageFormat : uint8_t {
    kMono1,
};

struct EmbeddedImageAsset {
    const uint8_t* data;
    uint16_t width;
    uint16_t height;
    uint16_t stride_bytes;
    ImageFormat format;
};

inline constexpr size_t PackedMonoBitmapSize(uint16_t width, uint16_t height) {
    return static_cast<size_t>((width + 7U) / 8U) * height;
}

#endif  // PROJECT_ASSET_TYPES_H
