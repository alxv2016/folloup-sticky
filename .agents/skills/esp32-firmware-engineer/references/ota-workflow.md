# OTA Workflow Reference

## Partition Layout Requirements

### Minimal 2-OTA Layout (4MB flash)
```csv
# Name,   Type, SubType, Offset,   Size,  Flags
nvs,      data, nvs,     0x9000,   0x6000,
otadata,  data, ota,     0xf000,   0x2000,
phy_init, data, phy,     0x11000,  0x1000,
ota_0,    app,  ota_0,   0x20000,  0x180000,
ota_1,    app,  ota_1,   0x1a0000, 0x180000,
```

### Factory + 2-OTA (preferred for rollback)
```csv
nvs,      data, nvs,     0x9000,   0x6000,
otadata,  data, ota,     0xf000,   0x2000,
phy_init, data, phy,     0x11000,  0x1000,
factory,  app,  factory, 0x20000,  0x100000,
ota_0,    app,  ota_0,   0x120000, 0x180000,
ota_1,    app,  ota_1,   0x2a0000, 0x180000,
```

### Key Rules
- `otadata` partition is mandatory — without it the bootloader cannot track active OTA slot.
- `ota_0` and `ota_1` must be the same size.
- Size each OTA slot from the actual binary size reported by `idf.py size` with margin (≥20%).
- Never use the `factory` subtype for an OTA slot; reserve it for the recovery/golden image.
- Set `CONFIG_PARTITION_TABLE_CUSTOM=y` and point `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` at your CSV.

## OTA Update API Flow

### Basic In-App OTA Sequence
```c
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_app_format.h"

esp_err_t perform_ota(const uint8_t *data, size_t total_size)
{
    const esp_partition_t *update_partition =
        esp_ota_get_next_update_partition(NULL);  // picks the inactive slot
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_handle_t handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    // Write data in chunks as received (e.g. from HTTP stream)
    err = esp_ota_write(handle, data, total_size);
    if (err != ESP_OK) {
        esp_ota_abort(handle);
        return err;
    }

    err = esp_ota_end(handle);  // validates the image
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s (image may be corrupt)", esp_err_to_name(err));
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA complete. Rebooting into new firmware.");
    esp_restart();
    return ESP_OK;  // unreachable
}
```

### HTTPS OTA (Recommended for Network Updates)
```c
#include "esp_https_ota.h"

void ota_task(void *arg)
{
    esp_http_client_config_t http_cfg = {
        .url = CONFIG_OTA_FIRMWARE_URL,
        .cert_pem = server_cert_pem_start,  // embed via component CMakeLists EMBED_TXTFILES
        .timeout_ms = 5000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
        .http_client_init_cb = NULL,
        .bulk_flash_erase = false,           // set true only for very large images
        .partial_http_download = false,
    };

    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "HTTPS OTA failed: %s", esp_err_to_name(err));
    }
    vTaskDelete(NULL);
}
```

### Streaming HTTPS OTA (Chunk-by-Chunk, for Progress Reporting)
```c
esp_https_ota_handle_t ota_handle;
esp_err_t err = esp_https_ota_begin(&ota_cfg, &ota_handle);

int image_len = esp_https_ota_get_image_size(ota_handle);
while (true) {
    err = esp_https_ota_perform(ota_handle);
    if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
    int written = esp_https_ota_get_image_len_read(ota_handle);
    ESP_LOGI(TAG, "OTA progress: %d / %d bytes", written, image_len);
}

if (esp_https_ota_is_complete_data_received(ota_handle)) {
    err = esp_https_ota_finish(ota_handle);
    if (err == ESP_OK) esp_restart();
} else {
    esp_https_ota_abort(ota_handle);
}
```

## Rollback and Anti-Rollback

### Enabling App Rollback
```
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

With rollback enabled, after `esp_ota_set_boot_partition()` + reboot, the new image boots in
`ESP_OTA_IMG_PENDING_VERIFY` state. The app **must** call:
```c
esp_ota_mark_app_valid_cancel_rollback();
```
before any watchdog or reboot triggers. If it does not, the bootloader rolls back to the previous
slot on the next boot.

### Rollback Decision Pattern
```c
void app_main(void)
{
    // Early: check if we're running a newly OTA'd image
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // Run diagnostics before committing
            if (self_test_passed()) {
                ESP_LOGI(TAG, "Self-test passed. Committing OTA image.");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "Self-test FAILED. Rolling back.");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
    // Continue normal app startup...
}
```

### Anti-Rollback (Security Counter)
Prevents downgrading to a vulnerable firmware version.
```
CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=y
CONFIG_BOOTLOADER_APP_SEC_VER=1         # increment with each security-relevant release
CONFIG_BOOTLOADER_EFUSE_SECURE_VERSION_SCHEME=COUNTER  # or DIGEST
```
- Increment `CONFIG_BOOTLOADER_APP_SEC_VER` only for security fixes — cannot be decremented.
- The bootloader reads the security version from eFuse and refuses to boot any image with a lower version.

## Key sdkconfig Options

| Option | Purpose |
|---|---|
| `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` | Enable automatic rollback if app does not self-validate |
| `CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK` | Reject firmware with lower security version counter |
| `CONFIG_BOOTLOADER_APP_SEC_VER` | Security version counter value baked into this build |
| `CONFIG_OTA_ALLOW_HTTP` | Allow plain HTTP for OTA (dev only — never in production) |
| `CONFIG_ESP_HTTPS_OTA_DECRYPT_CB` | Custom decryption callback for encrypted OTA images |
| `CONFIG_PARTITION_TABLE_CUSTOM` | Use project-specific partition CSV |

## Diagnostic Commands for OTA State

```c
// Log running, boot, and next-update partitions
const esp_partition_t *running  = esp_ota_get_running_partition();
const esp_partition_t *boot     = esp_ota_get_boot_partition();
const esp_partition_t *next     = esp_ota_get_next_update_partition(NULL);

ESP_LOGI(TAG, "Running: %s @ 0x%08" PRIx32, running->label, running->address);
ESP_LOGI(TAG, "Boot:    %s @ 0x%08" PRIx32, boot->label,    boot->address);
ESP_LOGI(TAG, "Update:  %s @ 0x%08" PRIx32, next->label,    next->address);

// Log OTA state of running partition
esp_ota_img_states_t state;
if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
    ESP_LOGI(TAG, "OTA state: %d (%s)", state,
        state == ESP_OTA_IMG_NEW            ? "NEW"            :
        state == ESP_OTA_IMG_PENDING_VERIFY ? "PENDING_VERIFY" :
        state == ESP_OTA_IMG_VALID          ? "VALID"          :
        state == ESP_OTA_IMG_INVALID        ? "INVALID"        :
        state == ESP_OTA_IMG_ABORTED        ? "ABORTED"        : "UNDEFINED");
}
```

## Common Failure Modes

| Symptom | Likely Cause |
|---|---|
| Bootloader always boots `ota_0` | `otadata` partition was erased or never written; run `idf.py erase-flash` and re-flash |
| Rollback on every boot | App never calls `esp_ota_mark_app_valid_cancel_rollback()` |
| `esp_ota_end` returns `ESP_ERR_OTA_VALIDATE_FAILED` | Image hash check failed — data corruption during transfer |
| HTTPS OTA fails with `ESP_ERR_HTTP_CONNECT` | Server cert not embedded or `cert_pem` pointer wrong |
| OTA slot too small | Binary grew past slot size — recalculate with `idf.py size` and widen CSV |
| `esp_ota_begin` fails with `ESP_ERR_INVALID_SIZE` | `image_size` parameter too small; use `OTA_WITH_SEQUENTIAL_WRITES` |

## OTA and Secure Boot

- Secure Boot verifies the image signature on boot; OTA images must be signed with the same key.
- Use `idf.py secure-target sign-data` or the build system's `--sign-key` path to sign the binary before serving it.
- With flash encryption enabled, the OTA partition is automatically encrypted on write — the plaintext binary URL is correct; the ESP32 encrypts in-place.
- Do not disable `CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC` or `CONFIG_SECURE_BOOT_ALLOW_JTAG` in development then forget to re-enable restrictions for production builds.
