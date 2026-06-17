#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Register a basic service terminal command set (help/status/settings/heap/tasks/log/reboot).
 *
 * Use ESP-IDF's REPL helpers (UART / USB CDC / USB-Serial-JTAG, depending target/IDF version)
 * to provide line editing, history, help, and autocomplete.
 */
esp_err_t app_console_register_commands(void);

#ifdef __cplusplus
}
#endif
