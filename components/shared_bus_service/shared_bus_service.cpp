#include "shared_bus_service.h"

#include <cstdint>

#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace shared_bus_service {
namespace {

constexpr const char* kTag = "SharedBusService";

SemaphoreHandle_t s_state_mutex = nullptr;
SemaphoreHandle_t s_bus_mutex = nullptr;
Owner s_owner = Owner::kNone;
TaskHandle_t s_owner_task = nullptr;
uint32_t s_owner_depth = 0;

esp_err_t EnsureMutexes()
{
    if (s_state_mutex == nullptr) {
        s_state_mutex = xSemaphoreCreateMutex();
        if (s_state_mutex == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_bus_mutex == nullptr) {
        s_bus_mutex = xSemaphoreCreateMutex();
        if (s_bus_mutex == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t AcquireOwner(Owner owner, TickType_t timeout)
{
    if (owner == Owner::kNone) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t init_err = EnsureMutexes();
    if (init_err != ESP_OK) {
        return init_err;
    }

    const TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (s_owner == owner && s_owner_task == current_task) {
        ++s_owner_depth;
        xSemaphoreGive(s_state_mutex);
        return ESP_OK;
    }
    xSemaphoreGive(s_state_mutex);

    if (xSemaphoreTake(s_bus_mutex, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) != pdTRUE) {
        xSemaphoreGive(s_bus_mutex);
        return ESP_FAIL;
    }

    if (s_owner != Owner::kNone) {
        ESP_LOGW(kTag, "Bus acquired while metadata owner=%s depth=%lu",
                 OwnerName(s_owner), static_cast<unsigned long>(s_owner_depth));
        xSemaphoreGive(s_state_mutex);
        xSemaphoreGive(s_bus_mutex);
        return ESP_FAIL;
    }

    s_owner = owner;
    s_owner_task = current_task;
    s_owner_depth = 1;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

void ReleaseOwner(Owner owner)
{
    if (owner == Owner::kNone || s_state_mutex == nullptr || s_bus_mutex == nullptr) {
        return;
    }

    const TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (s_owner != owner || s_owner_task != current_task || s_owner_depth == 0) {
        ESP_LOGW(kTag, "Release ignored for owner=%s current=%s depth=%lu",
                 OwnerName(owner), OwnerName(s_owner),
                 static_cast<unsigned long>(s_owner_depth));
        xSemaphoreGive(s_state_mutex);
        return;
    }

    bool release_bus = false;
    --s_owner_depth;
    if (s_owner_depth == 0) {
        s_owner = Owner::kNone;
        s_owner_task = nullptr;
        release_bus = true;
    }

    xSemaphoreGive(s_state_mutex);
    if (release_bus) {
        xSemaphoreGive(s_bus_mutex);
    }
}

bool IsOwnerActive(Owner owner)
{
    if (s_state_mutex == nullptr) {
        return false;
    }

    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    const bool active = s_owner == owner;
    xSemaphoreGive(s_state_mutex);
    return active;
}

}  // namespace

esp_err_t Init()
{
    return EnsureMutexes();
}

esp_err_t AcquireDisplay(TickType_t timeout)
{
    return AcquireOwner(Owner::kDisplay, timeout);
}

void ReleaseDisplay()
{
    ReleaseOwner(Owner::kDisplay);
}

esp_err_t AcquireStorage(TickType_t timeout)
{
    return AcquireOwner(Owner::kStorage, timeout);
}

void ReleaseStorage()
{
    ReleaseOwner(Owner::kStorage);
}

bool IsDisplayActive()
{
    return IsOwnerActive(Owner::kDisplay);
}

bool IsStorageActive()
{
    return IsOwnerActive(Owner::kStorage);
}

Owner CurrentOwner()
{
    if (s_state_mutex == nullptr) {
        return Owner::kNone;
    }

    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) != pdTRUE) {
        return Owner::kNone;
    }

    const Owner owner = s_owner;
    xSemaphoreGive(s_state_mutex);
    return owner;
}

const char* OwnerName(Owner owner)
{
    switch (owner) {
        case Owner::kNone:
            return "none";
        case Owner::kDisplay:
            return "display";
        case Owner::kStorage:
            return "storage";
        default:
            return "unknown";
    }
}

}  // namespace shared_bus_service
