#ifndef BUZZER_SERVICE_H_
#define BUZZER_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

namespace buzzer_service {

enum class Pattern {
    kStartup,
    kGeminiConnected,
    kClick,
    kLongClick,
    kDoubleClick,
    kError,
    kShutdown,
};

esp_err_t Init();
esp_err_t PlayTone(uint32_t frequency_hz, uint32_t duration_ms);
esp_err_t PlayPattern(Pattern pattern);
esp_err_t Stop();

}  // namespace buzzer_service

#endif  // BUZZER_SERVICE_H_
