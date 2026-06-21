#ifndef SHARED_BUS_SERVICE_H_
#define SHARED_BUS_SERVICE_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

namespace shared_bus_service {

enum class Owner {
    kNone,
    kDisplay,
    kStorage,
};

esp_err_t Init();

esp_err_t AcquireDisplay(TickType_t timeout = portMAX_DELAY);
void ReleaseDisplay();

esp_err_t AcquireStorage(TickType_t timeout = portMAX_DELAY);
void ReleaseStorage();

bool IsDisplayActive();
bool IsStorageActive();
Owner CurrentOwner();
const char* OwnerName(Owner owner);

}  // namespace shared_bus_service

#endif  // SHARED_BUS_SERVICE_H_
