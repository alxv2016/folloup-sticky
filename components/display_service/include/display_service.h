#ifndef DISPLAY_SERVICE_H_
#define DISPLAY_SERVICE_H_

#include "esp_err.h"

namespace display_service {

enum class DemoSelection {
    kTop,
    kBottom,
};

esp_err_t Init();
bool IsInitialized();
esp_err_t SelectDemoSelection(DemoSelection selection);

}  // namespace display_service

#endif  // DISPLAY_SERVICE_H_
