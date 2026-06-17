# LVGL + ESP-IDF Reference

## Version Compatibility Matrix

| LVGL | Minimum ESP-IDF | Notes |
|---|---|---|
| v8.3.x | 4.4+ | Stable, widely used; uses `lv_disp_drv_t` / `lv_indev_drv_t` API |
| v9.0.x | 5.0+ | Breaking API change from v8 — `lv_display_t`, new flush callback signature |
| v9.1+ | 5.1+ | Recommended for new projects on IDF 5.x |

**Always confirm the exact LVGL version in `idf_component.yml` or `CMakeLists.txt` before writing any driver or integration code. The v8→v9 API change is not backwards compatible.**

Obtain LVGL via the IDF Component Manager:
```yaml
# idf_component.yml
dependencies:
  lvgl/lvgl: "^9.1.0"
  # or for v8:
  # lvgl/lvgl: "^8.3.0"
```

Or via managed components:
```bash
idf.py add-dependency "lvgl/lvgl^9.1.0"
```

---

## Display Flush Callback (v9.x)

```c
#include "lvgl.h"
#include "driver/spi_master.h"

static spi_device_handle_t spi;

// Called by LVGL when a render area is ready to be sent to the display.
// Must call lv_display_flush_ready() when transfer is complete.
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    // Set the display window (controller-specific — example: ILI9341/ST7789 sequence)
    lcd_set_window(area->x1, area->y1, area->x2, area->y2);

    // DMA SPI transfer — px_map must be in DMA-capable memory
    spi_transaction_t t = {
        .length = w * h * 2 * 8,  // bits; 2 bytes per pixel for RGB565
        .tx_buffer = px_map,
        .flags = 0,
    };
    spi_device_queue_trans(spi, &t, portMAX_DELAY);

    // Signal LVGL that flush is done.
    // If using DMA with a callback, call this from the DMA completion ISR instead:
    lv_display_flush_ready(disp);
}
```

**v8.x Equivalent (different function signature):**
```c
static void disp_flush_v8(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    // ... transfer logic ...
    lv_disp_flush_ready(drv);  // note: lv_disp_flush_ready, not lv_display_flush_ready
}
```

---

## Display Initialization

### v9.x
```c
#include "lvgl.h"

#define DISP_WIDTH   320
#define DISP_HEIGHT  240
#define BUF_LINES    20   // number of lines in each draw buffer

static lv_display_t *disp;
static lv_color_t buf1[DISP_WIDTH * BUF_LINES];
static lv_color_t buf2[DISP_WIDTH * BUF_LINES];  // optional second buffer for double-buffering

void lvgl_display_init(void)
{
    lv_init();

    disp = lv_display_create(DISP_WIDTH, DISP_HEIGHT);
    lv_display_set_flush_cb(disp, disp_flush);

    // Single buffer:
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Double buffer (smoother rendering, uses 2x RAM):
    // lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Full-screen buffer in PSRAM (ESP32-S3 with PSRAM):
    // lv_color_t *fb = heap_caps_malloc(DISP_WIDTH * DISP_HEIGHT * sizeof(lv_color_t),
    //                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // lv_display_set_buffers(disp, fb, NULL, DISP_WIDTH * DISP_HEIGHT * sizeof(lv_color_t),
    //                        LV_DISPLAY_RENDER_MODE_FULL);

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
}
```

---

## Tick Source (Required)

LVGL requires a millisecond tick to animate, time events, and drive transitions.

### Option A: FreeRTOS Timer (Preferred)
```c
static void lvgl_tick_timer_cb(TimerHandle_t xTimer)
{
    lv_tick_inc(portTICK_PERIOD_MS);  // usually 1ms if configTICK_RATE_HZ=1000
}

void lvgl_tick_init(void)
{
    TimerHandle_t timer = xTimerCreate("lvgl_tick", pdMS_TO_TICKS(1),
                                       pdTRUE, NULL, lvgl_tick_timer_cb);
    xTimerStart(timer, 0);
}
```

### Option B: esp_timer (Higher Resolution)
```c
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(1);  // called every 1ms
}

void lvgl_tick_init(void)
{
    const esp_timer_create_args_t args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000));  // 1000µs = 1ms
}
```

---

## LVGL Task and Mutex (Thread Safety)

LVGL is **not thread-safe**. All `lv_` calls — including UI construction, style updates, and animations — must happen from the same task that calls `lv_timer_handler()`, or be protected by a mutex.

### Dedicated LVGL Task Pattern
```c
static SemaphoreHandle_t lvgl_mutex;

void lvgl_lock(void)   { xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY); }
void lvgl_unlock(void) { xSemaphoreGiveRecursive(lvgl_mutex); }

static void lvgl_task(void *arg)
{
    lvgl_tick_init();
    lvgl_display_init();
    ui_init();  // create screens, widgets, etc.

    while (true) {
        lvgl_lock();
        uint32_t time_to_next = lv_timer_handler();
        lvgl_unlock();
        vTaskDelay(pdMS_TO_TICKS(time_to_next > 5 ? 5 : time_to_next));
    }
}

void app_main(void)
{
    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, NULL,
                            1);  // pin to core 1; leave core 0 for Wi-Fi/BLE
}

// Updating UI from another task:
void update_label_from_task(lv_obj_t *label, const char *text)
{
    lvgl_lock();
    lv_label_set_text(label, text);
    lvgl_unlock();
}
```

---

## Color Format and Byte Order

**This is the most common source of wrong colors and washed-out display output.**

### Identify Your Controller's Expected Format
| Controller | Typical Format | Byte Order |
|---|---|---|
| ILI9341 | RGB565 | Big-endian (MSB first) |
| ST7789 | RGB565 | Big-endian |
| SH8601 | RGB888 or ARGB8888 | Depends on init |
| GC9A01 | RGB565 | Big-endian |
| RA8875 | RGB565 | Big-endian |

### Configuring LVGL Color Format
```c
// v9.x — set on the display object:
lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
// or
lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB888);
```

### Byte Swap for SPI Controllers
Most ESP32 SPI controllers transmit LSB-first by default; most display controllers expect big-endian RGB565. Fix with:
```c
// v9.x:
lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
// Enable byte swap in the display object (swaps bytes of each 16-bit pixel before flush):
// lv_display_set_byte_swap(disp, true);   // available in v9.1+

// Or: swap in hardware via SPI controller flag:
// .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY  -- check your IDF version
```

If colors are inverted (blue appears red), the byte order is wrong. If colors look correct but washed-out/dark, alpha channel or bit depth is wrong.

---

## Memory Configuration

### sdkconfig Options
```
# Increase task stack for LVGL rendering (default is often too small):
# The lvgl task itself: 8192–16384 bytes depending on widget complexity.

# Enable PSRAM for large frame buffers (ESP32-S3):
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y          # ESP32-S3 Octal PSRAM
CONFIG_SPIRAM_SPEED_80M=y

# Allow malloc from PSRAM:
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384  # keep small allocs in IRAM
```

### Allocating Draw Buffers
```c
// Internal SRAM (fast, limited — use for small buffers or when PSRAM unavailable):
static lv_color_t buf[LCD_WIDTH * 20];  // 20 lines

// PSRAM (ESP32-S3 — for large/full-frame buffers):
lv_color_t *buf = heap_caps_aligned_alloc(64,
    LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
assert(buf != NULL);

// DMA-capable (required for SPI DMA transfers — must NOT be in PSRAM on some targets):
lv_color_t *dma_buf = heap_caps_aligned_alloc(64,
    LCD_WIDTH * 20 * sizeof(lv_color_t),
    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
```

**On ESP32 (original):** DMA-capable memory is IRAM/DRAM; PSRAM is not DMA-capable for SPI.
**On ESP32-S3:** PSRAM can be used for SPI DMA with EDMA (check driver docs and `MALLOC_CAP_DMA`).

---

## Performance Tuning

### Double Buffering
Use two render buffers so LVGL can prepare the next frame while the DMA is transmitting the current one:
```c
lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
```
In the flush callback, start DMA and return immediately. Call `lv_display_flush_ready()` from the DMA completion callback — this allows LVGL to start rendering the next frame concurrently.

### Avoid Blocking in Flush Callback
```c
// Bad: blocks until transfer completes
spi_device_transmit(spi, &t);
lv_display_flush_ready(disp);

// Better: queue DMA, signal completion from ISR or polling callback
spi_device_queue_trans(spi, &t, portMAX_DELAY);
// lv_display_flush_ready() called from DMA complete CB
```

### SPI Frequency
- ILI9341/ST7789: typically 40–80MHz depending on board trace quality
- Start at 20MHz, increase until artifacts appear, then back off 10%
- Set via `spi_device_interface_config_t.clock_speed_hz`

### Dirty Region Rendering
LVGL only redraws changed regions. Avoid calling `lv_obj_invalidate()` on entire screens unnecessarily. Prefer updating individual labels, arcs, or images.

---

## Common Pitfalls

| Symptom | Cause | Fix |
|---|---|---|
| Screen all white/black after init | Flush callback never called or controller not initialized | Verify `lv_timer_handler()` is called; check display init sequence |
| Colors wrong (blue ↔ red) | RGB byte order mismatch | Enable byte swap or swap R/B in flush callback |
| Colors washed out / dark | Wrong color depth (e.g. 24-bit data to 16-bit controller) | Match `lv_display_set_color_format()` to controller |
| Crash in flush callback | Draw buffer not in DMA-capable memory | Use `MALLOC_CAP_DMA\|MALLOC_CAP_INTERNAL` for SPI DMA buffers |
| Flickering / tearing | Single buffer, no vsync | Use double buffer; add DMA completion signaling |
| UI locks up after a few updates | `lv_timer_handler()` blocked or mutex deadlock | Ensure LVGL task runs without blocking; check mutex acquire/release pairing |
| `lv_tick_inc` not called | No tick source configured | Add FreeRTOS timer or `esp_timer` calling `lv_tick_inc(1)` every 1ms |
| Animations stutter | `lv_timer_handler()` called too infrequently | Cap sleep to 5ms; don't `vTaskDelay(time_to_next)` with large values |
| Stack overflow in LVGL task | Complex widgets exceed task stack | Increase task stack to 16384+ bytes for complex UIs |

---

## LVGL + ESP-IDF Component Manager Lock File

After resolving a working combination, record it in the project compatibility lock file:

```yaml
# esp-framework-compat.lock
esp-idf: "v5.2.1"
lvgl: "v9.1.0"   # from idf_component.yml / managed_components
display-controller: "ST7789"
notes: "RGB565, big-endian, 40MHz SPI, double-buffer DMA on ESP32-S3 with Octal PSRAM"
verified: "2025-01-15"
```
