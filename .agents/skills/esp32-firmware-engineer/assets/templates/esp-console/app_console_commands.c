#include "app_console_commands.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_console";

static const char *app_reset_reason_to_str(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_UNKNOWN: return "unknown";
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "ext";
    case ESP_RST_SW: return "sw";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "other";
    }
}

static esp_log_level_t app_parse_log_level(const char *s, bool *ok)
{
    *ok = true;
    if (strcmp(s, "e") == 0 || strcmp(s, "error") == 0) return ESP_LOG_ERROR;
    if (strcmp(s, "w") == 0 || strcmp(s, "warn") == 0 || strcmp(s, "warning") == 0) return ESP_LOG_WARN;
    if (strcmp(s, "i") == 0 || strcmp(s, "info") == 0) return ESP_LOG_INFO;
    if (strcmp(s, "d") == 0 || strcmp(s, "debug") == 0) return ESP_LOG_DEBUG;
    if (strcmp(s, "v") == 0 || strcmp(s, "verbose") == 0) return ESP_LOG_VERBOSE;
    *ok = false;
    return ESP_LOG_NONE;
}

/*
 * Weak hooks let projects back the terminal with real application settings storage
 * without editing the command parser.
 */
__attribute__((weak)) int app_console_settings_get(const char *key)
{
    printf("settings.get not implemented for key='%s'\n", key);
    return 0;
}

__attribute__((weak)) int app_console_settings_set(const char *key, const char *value)
{
    printf("settings.set not implemented for key='%s' value='%s'\n", key, value);
    return 0;
}

__attribute__((weak)) int app_console_settings_save(void)
{
    printf("settings.save not implemented\n");
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    esp_chip_info_t chip_info = {0};
    esp_chip_info(&chip_info);

    printf("uptime_ms=%lld\n", (long long)(esp_timer_get_time() / 1000));
    printf("reset_reason=%s\n", app_reset_reason_to_str(esp_reset_reason()));
    printf("cores=%d features=0x%x revision=%d\n",
           chip_info.cores, chip_info.features, chip_info.revision);
    return 0;
}

static int cmd_heap(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    size_t free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t min_8bit = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    size_t largest_8bit = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    printf("heap_8bit_free=%u min=%u largest=%u\n",
           (unsigned)free_8bit, (unsigned)min_8bit, (unsigned)largest_8bit);
    return 0;
}

static int cmd_tasks(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("num_tasks=%u\n", (unsigned)uxTaskGetNumberOfTasks());

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
    printf("current_task_stack_hwm_words=%u\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
#else
    printf("stack_hwm_unavailable (enable INCLUDE_uxTaskGetStackHighWaterMark)\n");
#endif

    printf("tip=extend this command with per-task snapshots when trace/stats config is enabled\n");
    return 0;
}

static int cmd_log(int argc, char **argv)
{
    if (argc != 4 || strcmp(argv[1], "level") != 0) {
        printf("usage: log level <tag|*> <error|warn|info|debug|verbose>\n");
        return 1;
    }

    bool ok = false;
    esp_log_level_t level = app_parse_log_level(argv[3], &ok);
    if (!ok) {
        printf("invalid level '%s'\n", argv[3]);
        return 1;
    }

    esp_log_level_set(argv[2], level);
    printf("log_level_set tag=%s level=%s\n", argv[2], argv[3]);
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("restarting...\n");
    fflush(stdout);
    esp_restart();
    return 0;
}

static int cmd_settings(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage:\n");
        printf("  settings get <key>\n");
        printf("  settings set <key> <value>\n");
        printf("  settings save\n");
        return 1;
    }

    if (strcmp(argv[1], "get") == 0) {
        if (argc != 3) {
            printf("usage: settings get <key>\n");
            return 1;
        }
        return app_console_settings_get(argv[2]);
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 4) {
            printf("usage: settings set <key> <value>\n");
            return 1;
        }
        return app_console_settings_set(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "save") == 0) {
        if (argc != 2) {
            printf("usage: settings save\n");
            return 1;
        }
        return app_console_settings_save();
    }

    printf("unknown settings subcommand '%s'\n", argv[1]);
    return 1;
}

static esp_err_t app_register_cmd(const char *name, const char *help,
                                  esp_console_cmd_func_t func)
{
    const esp_console_cmd_t cmd = {
        .command = name,
        .help = help,
        .hint = NULL,
        .func = func,
        .argtable = NULL,
    };
    return esp_console_cmd_register(&cmd);
}

esp_err_t app_console_register_commands(void)
{
    esp_err_t err = esp_console_register_help_command();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    ESP_ERROR_CHECK(app_register_cmd("status",
                                     "Show uptime, reset reason, and chip summary",
                                     cmd_status));
    ESP_ERROR_CHECK(app_register_cmd("heap",
                                     "Show heap free/min/largest block summary",
                                     cmd_heap));
    ESP_ERROR_CHECK(app_register_cmd("tasks",
                                     "Show RTOS task/debug summary",
                                     cmd_tasks));
    ESP_ERROR_CHECK(app_register_cmd("settings",
                                     "Get/set/save application settings",
                                     cmd_settings));
    ESP_ERROR_CHECK(app_register_cmd("log",
                                     "Runtime log control: log level <tag|*> <level>",
                                     cmd_log));
    ESP_ERROR_CHECK(app_register_cmd("reboot",
                                     "Restart the device",
                                     cmd_reboot));

    ESP_LOGI(TAG, "service terminal commands registered");
    return ESP_OK;
}
