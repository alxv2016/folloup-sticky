# Security Hardening Reference

## Security Feature Overview

| Feature | Where Configured | Reversible? | Production Required? |
|---|---|---|---|
| Secure Boot v2 | eFuse + sdkconfig | No (eFuse burn) | Yes for signed field devices |
| Flash Encryption | eFuse + sdkconfig | No (Development mode only) | Yes for sensitive data |
| NVS Encryption | sdkconfig + key partition | Yes (key erasable) | If NVS holds secrets |
| JTAG Disable | eFuse | No | Yes for production |
| UART Download Disable | eFuse | No | Yes for tamper resistance |
| Service Terminal Auth | App code | Yes | Required if terminal exposed in production |

**Burn eFuse bits only after testing in Development mode. Release mode eFuse burns are permanent and irreversible.**

---

## Secure Boot v2

Verifies every stage of the boot chain (bootloader → app) using RSA-PSS or ECDSA signatures.

### Generate Signing Key
```bash
espsecure.py generate_signing_key --version 2 --scheme rsa3072 secure_boot_signing_key.pem
# Keep secure_boot_signing_key.pem offline and in a secrets manager. Never commit it.
```

### sdkconfig Settings (Development — key burned via idf.py)
```
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"
CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y
# Development: allows reflashing
CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC=y   # disable for production
CONFIG_SECURE_BOOT_ALLOW_JTAG=y        # disable for production
```

### sdkconfig Settings (Production)
```
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"
CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y
CONFIG_SECURE_BOOTLOADER_NO_REBOOT_ON_FAILURE=y  # brick if verification fails
# CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC is NOT set
# CONFIG_SECURE_BOOT_ALLOW_JTAG is NOT set
```

### First Flash with Secure Boot
```bash
idf.py build
# Bootloader is signed automatically at build time with the configured key.
idf.py -p /dev/ttyUSB0 flash   # Burns Secure Boot eFuse on first successful boot
```

### Signing OTA Images
OTA images must be signed with the same key used for the bootloader:
```bash
espsecure.py sign_data --version 2 --keyfile secure_boot_signing_key.pem \
    --output firmware_signed.bin build/firmware.bin
```

---

## Flash Encryption

Encrypts all flash contents (bootloader, app, NVS, OTA partitions) using AES-XTS-256.

### Development Mode (Reversible via reflash)
```
CONFIG_FLASH_ENCRYPTION_ENABLED=y
CONFIG_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y
```
- ESP32 generates a random key and burns it to eFuse on first encrypted boot.
- You can still reflash in development mode using `idf.py encrypted-flash`.
- The plaintext binary is encrypted before writing.

### Release Mode (Permanent — production only)
```
CONFIG_FLASH_ENCRYPTION_ENABLED=y
CONFIG_FLASH_ENCRYPTION_MODE_RELEASE=y
```
- Disables UART download mode permanently.
- No more plaintext reflashing after this eFuse is burned.

### Flash Encrypted Build + Flash Workflow
```bash
idf.py build
idf.py -p /dev/ttyUSB0 encrypted-flash  # initial flash (pre-encryption)
# After first boot, flash is encrypted; subsequent OTA goes through esp_ota_ops normally
```

### Pre-encrypting Binaries for Factory Programming
```bash
# Get the device's flash encryption key (already burned to eFuse in development mode):
espefuse.py -p /dev/ttyUSB0 burn_key BLOCK_KEY0 flash_encryption_key.bin FLASH_ENCRYPTION

# Encrypt a binary offline (for factory programming without serial access):
espsecure.py encrypt_flash_data --aes-xts --keyfile flash_encryption_key.bin \
    --address 0x10000 --output app_encrypted.bin build/app.bin
```

---

## NVS Encryption

Encrypts NVS partition contents using AES-XTS. Protects credentials, calibration data, and secrets stored in NVS.

### Generate NVS Encryption Key Partition
```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \
    generate-key --keytype XTS_AES_256 --key_protect_hmac \
    --kp_hmac_keygen --kp_hmac_keyfile hmac_key.bin \
    --kp_hmac_inputkey nvs_key_partition.bin
```

### sdkconfig Settings
```
CONFIG_NVS_ENCRYPTION=y
CONFIG_NVS_SEC_KEY_PROTECTION_SCHEME_HMAC=y  # or _FLASH_ENC if using flash encryption
```

### Initializing NVS with Encryption at Runtime
```c
#include "nvs_flash.h"
#include "nvs_sec_provider.h"

nvs_sec_cfg_t nvs_sec_cfg;
nvs_sec_scheme_t *sec_scheme_handle = NULL;

// Register the HMAC-based security scheme
ESP_ERROR_CHECK(nvs_sec_provider_register_hmac(&nvs_sec_cfg, &sec_scheme_handle));

// Init NVS with the encryption scheme
esp_err_t err = nvs_flash_init_with_sec_cfg(&nvs_sec_cfg);
if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init_with_sec_cfg(&nvs_sec_cfg);
}
ESP_ERROR_CHECK(err);
```

---

## Disabling Debug Interfaces

### JTAG (via eFuse)
```bash
# Check current JTAG eFuse state first:
espefuse.py -p /dev/ttyUSB0 summary

# Permanently disable JTAG (irreversible):
espefuse.py -p /dev/ttyUSB0 burn_efuse JTAG_DISABLE
```

Or via sdkconfig (burned automatically at first boot with Secure Boot Release mode):
```
# CONFIG_SECURE_BOOT_ALLOW_JTAG is not set
```

### UART Download Mode
```
CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC=n  # prevents ROM serial downloader in secure boot
# For full disable (Release flash encryption also disables this):
CONFIG_ESP_CONSOLE_UART_NONE=y        # removes console UART entirely (extreme hardening)
```

---

## Service Terminal Hardening

The on-device service terminal (see `references/device-terminal-console.md`) must be controlled in production builds.

### Compile-Time Removal
```c
// In app_console_commands.c or main.c:
#ifdef CONFIG_APP_SERVICE_TERMINAL_ENABLE
    app_console_init();
#endif
```
```
# sdkconfig.defaults for production:
# CONFIG_APP_SERVICE_TERMINAL_ENABLE is not set
```

### Runtime Authentication (if terminal must remain in production)
```c
static bool terminal_authenticated = false;

static int cmd_auth(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: auth <token>\n");
        return 1;
    }
    // Use constant-time comparison to avoid timing attacks
    const char *expected = config_get_terminal_token();  // from encrypted NVS
    if (expected && strlen(argv[1]) == strlen(expected) &&
        memcmp(argv[1], expected, strlen(expected)) == 0) {
        terminal_authenticated = true;
        printf("Authenticated.\n");
        return 0;
    }
    printf("Authentication failed.\n");
    vTaskDelay(pdMS_TO_TICKS(2000));  // rate-limit brute force
    return 1;
}

// Guard all sensitive commands:
static int cmd_settings(int argc, char **argv)
{
    if (!terminal_authenticated) {
        printf("Not authenticated. Run: auth <token>\n");
        return 1;
    }
    // ... settings logic
}
```

---

## Secure Coding Practices

### Stack Canaries
```
CONFIG_COMPILER_STACK_CHECK_MODE_NORM=y   # adds __stack_chk_guard checks
# or stronger:
CONFIG_COMPILER_STACK_CHECK_MODE_STRONG=y
```

### Heap Integrity Checks (Development/QA builds)
```
CONFIG_HEAP_POISONING_COMPREHENSIVE=y  # expensive, use for test builds only
CONFIG_HEAP_TASK_TRACKING=y
```

### Assert Behavior
```
# Development: abort on assert failure (captures stack trace)
CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL=2

# Production: log + reset (avoids exposing stack trace externally)
CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL=1
```

### TLS Certificate Pinning for HTTPS OTA
```c
// Embed server certificate in the firmware binary:
// In CMakeLists.txt:
//   target_add_binary_data(${COMPONENT_LIB} "server_cert.pem" TEXT)

extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_server_cert_pem_end");

esp_http_client_config_t cfg = {
    .url = OTA_URL,
    .cert_pem = (const char *)server_cert_pem_start,
    // .use_global_ca_store = false,  // do not use — pin to specific cert
    .skip_cert_common_name_check = false,
};
```

### Sensitive Data Lifetime
- Zero secrets in RAM after use: `explicit_bzero(buf, len)` or `memset` + compiler barrier.
- Do not log credentials, tokens, or key material at any log level.
- Store secrets in NVS with encryption enabled — never in SPIFFS or plain NVS.

---

## Production Build Checklist

- [ ] Secure Boot v2 enabled; signing key stored offline (not in repo)
- [ ] Flash Encryption in Release mode (or Development mode for engineering builds)
- [ ] NVS Encryption enabled for all secret/credential namespaces
- [ ] JTAG disabled via eFuse or Secure Boot Release policy
- [ ] UART download mode disabled (Release flash encryption) or ROM basic disabled
- [ ] Service terminal removed or auth-gated behind a credential from encrypted NVS
- [ ] OTA images signed with Secure Boot key before serving
- [ ] Anti-rollback counter set and `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`
- [ ] TLS certificate pinned for all HTTPS connections (OTA, cloud, etc.)
- [ ] Stack canaries enabled (`CONFIG_COMPILER_STACK_CHECK_MODE_NORM`)
- [ ] No debug symbols or verbose logs in release build (`CONFIG_LOG_DEFAULT_LEVEL_WARN` or higher)
- [ ] No `CONFIG_OTA_ALLOW_HTTP=y` in production sdkconfig
- [ ] `espefuse.py summary` run and verified before shipping; no unexpected eFuse bits set
