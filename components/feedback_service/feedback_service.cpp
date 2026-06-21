#include "feedback_service.h"

#include "buzzer_service.h"
#include "esp_log.h"

namespace feedback_service {
namespace {

constexpr const char* kTag = "FeedbackService";

buzzer_service::Pattern PatternForEvent(FeedbackEvent event)
{
    switch (event) {
        case FeedbackEvent::kStartup:
            return buzzer_service::Pattern::kStartup;
        case FeedbackEvent::kGeminiConnected:
            return buzzer_service::Pattern::kGeminiConnected;
        case FeedbackEvent::kLock:
            return buzzer_service::Pattern::kLock;
        case FeedbackEvent::kUnlock:
            return buzzer_service::Pattern::kUnlock;
        case FeedbackEvent::kButtonClick:
        case FeedbackEvent::kTouchContact:
            return buzzer_service::Pattern::kClick;
        case FeedbackEvent::kButtonDoubleClick:
            return buzzer_service::Pattern::kDoubleClick;
        case FeedbackEvent::kButtonLongPress:
            return buzzer_service::Pattern::kLongClick;
        case FeedbackEvent::kShutdown:
            return buzzer_service::Pattern::kShutdown;
        case FeedbackEvent::kError:
        default:
            return buzzer_service::Pattern::kError;
    }
}

const char* FeedbackEventName(FeedbackEvent event)
{
    switch (event) {
        case FeedbackEvent::kStartup:
            return "startup";
        case FeedbackEvent::kGeminiConnected:
            return "gemini_connected";
        case FeedbackEvent::kLock:
            return "lock";
        case FeedbackEvent::kUnlock:
            return "unlock";
        case FeedbackEvent::kButtonClick:
            return "button_click";
        case FeedbackEvent::kButtonDoubleClick:
            return "button_double_click";
        case FeedbackEvent::kButtonLongPress:
            return "button_long_press";
        case FeedbackEvent::kTouchContact:
            return "touch_contact";
        case FeedbackEvent::kShutdown:
            return "shutdown";
        case FeedbackEvent::kError:
            return "error";
        default:
            return "unknown";
    }
}

}  // namespace

esp_err_t Init()
{
    const esp_err_t err = buzzer_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Buzzer service init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "Feedback service initialized");
    return ESP_OK;
}

esp_err_t Play(FeedbackEvent event)
{
    const esp_err_t err = buzzer_service::PlayPattern(PatternForEvent(event));
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Feedback event %s failed: %s",
                 FeedbackEventName(event), esp_err_to_name(err));
    }
    return err;
}

}  // namespace feedback_service
