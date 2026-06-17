#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "display_flush";

typedef enum {
    DISPLAY_FMT_RGB565_LE,
    DISPLAY_FMT_RGB565_BE,
    DISPLAY_FMT_RGB888,
} display_pixel_format_t;

typedef struct {
    int width;
    int height;
    display_pixel_format_t pixel_format;
    bool bgr_order;
} display_caps_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    const void *pixels;
    size_t len_bytes;
} display_flush_region_t;

esp_err_t display_flush_checked(const display_caps_t *caps, const display_flush_region_t *r)
{
    if (caps == NULL || r == NULL || r->pixels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (r->x < 0 || r->y < 0 || r->w <= 0 || r->h <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((r->x + r->w) > caps->width || (r->y + r->h) > caps->height) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t bpp = 0;
    switch (caps->pixel_format) {
    case DISPLAY_FMT_RGB565_LE:
    case DISPLAY_FMT_RGB565_BE:
        bpp = 2;
        break;
    case DISPLAY_FMT_RGB888:
        bpp = 3;
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    size_t expected = (size_t)r->w * (size_t)r->h * bpp;
    if (r->len_bytes != expected) {
        ESP_LOGE(TAG, "flush size mismatch: got=%u expected=%u", (unsigned)r->len_bytes,
                 (unsigned)expected);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGD(TAG, "flush x=%d y=%d w=%d h=%d fmt=%d bgr=%d", r->x, r->y, r->w, r->h,
             (int)caps->pixel_format, caps->bgr_order);

    /* Replace with actual panel transaction and DMA-safe buffer handling. */
    return ESP_OK;
}
