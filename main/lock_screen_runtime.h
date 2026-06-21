#ifndef LOCK_SCREEN_RUNTIME_H_
#define LOCK_SCREEN_RUNTIME_H_

#include "esp_err.h"

namespace lock_screen_runtime {

esp_err_t Init();
bool IsActive();
esp_err_t Show();
esp_err_t Hide();
esp_err_t Toggle();
esp_err_t SyncClockState(bool request_refresh_if_active);

}  // namespace lock_screen_runtime

#endif  // LOCK_SCREEN_RUNTIME_H_
