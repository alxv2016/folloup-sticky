#include "buzzer_service.h"

#include <atomic>
#include <cstddef>

#include "driver/ledc.h"
#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sticky_board_config.h"

namespace buzzer_service {
namespace {

constexpr const char* kTag = "BuzzerService";
constexpr ledc_mode_t kLedcMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kLedcTimer = LEDC_TIMER_0;
constexpr ledc_channel_t kLedcChannel = LEDC_CHANNEL_0;
constexpr ledc_timer_bit_t kLedcDutyResolution = LEDC_TIMER_10_BIT;
constexpr uint32_t kDefaultFrequencyHz = 4000;
constexpr uint32_t kMaxVolumeDuty = 512;
constexpr uint32_t kTonePollMs = 10;
constexpr uint32_t kQueueLength = 8;
constexpr uint32_t kWorkerStackBytes = 3072;

struct ToneStep {
    uint32_t frequency_hz;
    uint32_t duration_ms;
    uint32_t pause_after_ms;
};

enum class CommandType {
    kTone,
    kPattern,
    kStop,
};

struct Command {
    CommandType type = CommandType::kStop;
    Pattern pattern = Pattern::kStartup;
    uint32_t frequency_hz = 0;
    uint32_t duration_ms = 0;
};

QueueHandle_t s_command_queue = nullptr;
TaskHandle_t s_worker_task = nullptr;
bool s_initialized = false;
std::atomic_bool s_cancel_requested{false};

esp_err_t StopTone()
{
    esp_err_t err = ledc_set_duty(kLedcMode, kLedcChannel, 0);
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(kLedcMode, kLedcChannel);
}

esp_err_t StartTone(uint32_t frequency_hz)
{
    if (frequency_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ledc_set_freq(kLedcMode, kLedcTimer, frequency_hz);
    if (err != ESP_OK) {
        return err;
    }

    // For a passive PWM buzzer, the loudest useful drive is a 50% square wave.
    // 100% duty would be DC and would not produce the intended tone.
    err = ledc_set_duty(kLedcMode, kLedcChannel, kMaxVolumeDuty);
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(kLedcMode, kLedcChannel);
}

void DelayWithCancel(uint32_t duration_ms)
{
    uint32_t elapsed_ms = 0;
    while (elapsed_ms < duration_ms && !s_cancel_requested.load()) {
        const uint32_t remaining_ms = duration_ms - elapsed_ms;
        const uint32_t step_ms = remaining_ms < kTonePollMs ? remaining_ms : kTonePollMs;
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        elapsed_ms += step_ms;
    }
}

void PlayToneBlocking(uint32_t frequency_hz, uint32_t duration_ms)
{
    if (frequency_hz == 0 || duration_ms == 0) {
        return;
    }

    esp_err_t err = StartTone(frequency_hz);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to start tone %lu Hz: %s",
                 static_cast<unsigned long>(frequency_hz), esp_err_to_name(err));
        return;
    }

    DelayWithCancel(duration_ms);
    err = StopTone();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to stop tone: %s", esp_err_to_name(err));
    }
}

void PlaySteps(const ToneStep* steps, size_t count)
{
    if (steps == nullptr) {
        return;
    }

    for (size_t i = 0; i < count && !s_cancel_requested.load(); ++i) {
        PlayToneBlocking(steps[i].frequency_hz, steps[i].duration_ms);
        if (steps[i].pause_after_ms > 0 && !s_cancel_requested.load()) {
            DelayWithCancel(steps[i].pause_after_ms);
        }
    }
}

void PlayPatternBlocking(Pattern pattern)
{
    constexpr ToneStep kStartup[] = {
        {523, 120, 60},
        {659, 120, 60},
        {784, 120, 60},
        {1047, 180, 0},
    };
    constexpr ToneStep kGeminiConnected[] = {
        {1175, 70, 35},
        {1568, 90, 45},
        {2093, 130, 0},
    };
    constexpr ToneStep kLock[] = {
        {1047, 28, 12},
        {784, 28, 0},
    };
    constexpr ToneStep kUnlock[] = {
        {784, 28, 12},
        {1047, 28, 0},
    };
    constexpr ToneStep kClick[] = {
        {1760, 35, 0},
    };
    constexpr ToneStep kLongClick[] = {
        {988, 90, 0},
    };
    constexpr ToneStep kDoubleClick[] = {
        {1760, 35, 35},
        {1760, 35, 0},
    };
    constexpr ToneStep kError[] = {
        {220, 120, 70},
        {196, 180, 0},
    };
    constexpr ToneStep kShutdown[] = {
        {784, 90, 60},
        {523, 140, 0},
    };

    switch (pattern) {
        case Pattern::kStartup:
            PlaySteps(kStartup, sizeof(kStartup) / sizeof(kStartup[0]));
            break;
        case Pattern::kGeminiConnected:
            PlaySteps(kGeminiConnected,
                      sizeof(kGeminiConnected) / sizeof(kGeminiConnected[0]));
            break;
        case Pattern::kLock:
            PlaySteps(kLock, sizeof(kLock) / sizeof(kLock[0]));
            break;
        case Pattern::kUnlock:
            PlaySteps(kUnlock, sizeof(kUnlock) / sizeof(kUnlock[0]));
            break;
        case Pattern::kClick:
            PlaySteps(kClick, sizeof(kClick) / sizeof(kClick[0]));
            break;
        case Pattern::kLongClick:
            PlaySteps(kLongClick, sizeof(kLongClick) / sizeof(kLongClick[0]));
            break;
        case Pattern::kDoubleClick:
            PlaySteps(kDoubleClick, sizeof(kDoubleClick) / sizeof(kDoubleClick[0]));
            break;
        case Pattern::kError:
            PlaySteps(kError, sizeof(kError) / sizeof(kError[0]));
            break;
        case Pattern::kShutdown:
            PlaySteps(kShutdown, sizeof(kShutdown) / sizeof(kShutdown[0]));
            break;
    }
}

void WorkerTask(void*)
{
    Command command = {};
    while (true) {
        if (xQueueReceive(s_command_queue, &command, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (command.type == CommandType::kStop) {
            s_cancel_requested.store(false);
            (void)StopTone();
            continue;
        }

        s_cancel_requested.store(false);
        if (command.type == CommandType::kTone) {
            ESP_LOGI(kTag, "Playing tone: %lu Hz for %lu ms",
                     static_cast<unsigned long>(command.frequency_hz),
                     static_cast<unsigned long>(command.duration_ms));
            PlayToneBlocking(command.frequency_hz, command.duration_ms);
        } else if (command.type == CommandType::kPattern) {
            ESP_LOGI(kTag, "Playing pattern: %d", static_cast<int>(command.pattern));
            PlayPatternBlocking(command.pattern);
        }
    }
}

esp_err_t SendCommand(const Command& command, bool clear_queue)
{
    if (!s_initialized || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (clear_queue) {
        xQueueReset(s_command_queue);
    }

    if (xQueueSend(s_command_queue, &command, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = kLedcMode;
    timer_config.timer_num = kLedcTimer;
    timer_config.duty_resolution = kLedcDutyResolution;
    timer_config.freq_hz = kDefaultFrequencyHz;
    timer_config.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "LEDC timer init failed: %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t channel_config = {};
    channel_config.gpio_num = STICKY_BUZZER_PIN;
    channel_config.speed_mode = kLedcMode;
    channel_config.channel = kLedcChannel;
    channel_config.intr_type = LEDC_INTR_DISABLE;
    channel_config.timer_sel = kLedcTimer;
    channel_config.duty = 0;
    channel_config.hpoint = 0;

    err = ledc_channel_config(&channel_config);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "LEDC channel init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_command_queue = xQueueCreate(kQueueLength, sizeof(Command));
    if (s_command_queue == nullptr) {
        ESP_LOGW(kTag, "Failed to create command queue");
        return ESP_ERR_NO_MEM;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        WorkerTask,
        "buzzer",
        kWorkerStackBytes,
        nullptr,
        followup_task_config::kPriorityBuzzer,
        &s_worker_task,
        followup_task_config::kAppCore);
    if (created != pdPASS) {
        vQueueDelete(s_command_queue);
        s_command_queue = nullptr;
        s_worker_task = nullptr;
        ESP_LOGW(kTag, "Failed to create worker task");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(kTag, "Buzzer initialized on GPIO%d", STICKY_BUZZER_PIN);
    return ESP_OK;
}

esp_err_t PlayTone(uint32_t frequency_hz, uint32_t duration_ms)
{
    if (frequency_hz == 0 || duration_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    Command command = {};
    command.type = CommandType::kTone;
    command.frequency_hz = frequency_hz;
    command.duration_ms = duration_ms;
    return SendCommand(command, false);
}

esp_err_t PlayPattern(Pattern pattern)
{
    Command command = {};
    command.type = CommandType::kPattern;
    command.pattern = pattern;
    return SendCommand(command, false);
}

esp_err_t Stop()
{
    if (!s_initialized) {
        return ESP_OK;
    }

    s_cancel_requested.store(true);
    esp_err_t err = StopTone();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Immediate stop failed: %s", esp_err_to_name(err));
    }

    Command command = {};
    command.type = CommandType::kStop;
    const esp_err_t send_err = SendCommand(command, true);
    return send_err == ESP_OK ? err : send_err;
}

}  // namespace buzzer_service
