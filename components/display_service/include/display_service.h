#ifndef DISPLAY_SERVICE_H_
#define DISPLAY_SERVICE_H_

#include "esp_err.h"

namespace display_service {

esp_err_t Init();
bool IsInitialized();
esp_err_t EnterDisplaySleep();
esp_err_t EnterLightSleep();
esp_err_t WakeDisplay();
esp_err_t RecoverAfterLightSleep();
bool IsRefreshInProgress();

}  // namespace display_service

#endif  // DISPLAY_SERVICE_H_
