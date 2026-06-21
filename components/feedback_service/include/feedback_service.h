#ifndef FEEDBACK_SERVICE_H_
#define FEEDBACK_SERVICE_H_

#include "esp_err.h"

namespace feedback_service {

enum class FeedbackEvent {
    kStartup,
    kGeminiConnected,
    kButtonClick,
    kButtonDoubleClick,
    kButtonLongPress,
    kTouchContact,
    kShutdown,
    kError,
};

esp_err_t Init();
esp_err_t Play(FeedbackEvent event);

}  // namespace feedback_service

#endif  // FEEDBACK_SERVICE_H_
