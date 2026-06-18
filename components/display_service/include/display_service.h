#ifndef DISPLAY_SERVICE_H_
#define DISPLAY_SERVICE_H_

#include "esp_err.h"

namespace display_service {

esp_err_t Init();
bool IsInitialized();

}  // namespace display_service

#endif  // DISPLAY_SERVICE_H_
