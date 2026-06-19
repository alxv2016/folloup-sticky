#ifndef STORAGE_SERVICE_H_
#define STORAGE_SERVICE_H_

#include "esp_err.h"

namespace storage_service {

esp_err_t Init();
void LogDebugStatus();
bool IsMounted();
const char* MountPoint();

}  // namespace storage_service

#endif  // STORAGE_SERVICE_H_
