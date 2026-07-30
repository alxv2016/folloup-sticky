#include "power_key_runtime.h"

#include <mutex>

#include "axp2101.h"
#include "device_sleep_runtime.h"
#include "device_sleep_service.h"
#include "esp_log.h"
#include "waveshare_board.h"

namespace power_key_runtime {
namespace {

constexpr const char* kTag = "PowerKeyRuntime";

std::mutex s_mutex;
PressHandler s_handler = nullptr;
void* s_handler_context = nullptr;

// Long wins when both bits are set: one hold can latch the short IRQ on the way past the
// long threshold, and acting on both would fire two different actions for one press.
bool DecodePress(uint64_t irq_status, Press* press)
{
    if ((irq_status & XPOWERS_AXP2101_PKEY_LONG_IRQ) != 0) {
        *press = Press::kLong;
        return true;
    }
    if ((irq_status & XPOWERS_AXP2101_PKEY_SHORT_IRQ) != 0) {
        *press = Press::kShort;
        return true;
    }
    return false;
}

// A press that arrives while the device is display- or light-sleeping only wakes it.
// Without this the press that wakes the device would also run its action -- the same
// suppression device_sleep_runtime applies to the ACTION wake gesture.
bool ConsumeAsWake()
{
    const device_sleep_service::Stage stage =
        device_sleep_service::GetSnapshot().runtime.stage;
    device_sleep_runtime::NotifyUserActivity();
    if (stage == device_sleep_service::Stage::kAwake) {
        return false;
    }
    ESP_LOGI(kTag, "Power key consumed as wake from stage=%d", static_cast<int>(stage));
    return true;
}

// Runs on the PMIC driver's own IRQ task, not in ISR context, and the driver has already
// read and cleared the status register before calling us.
void HandleInterrupt(const Axp2101::InterruptEvent& event)
{
    Press press = Press::kShort;
    if (!DecodePress(event.irq_status, &press)) {
        return;
    }
    if (ConsumeAsWake()) {
        return;
    }

    PressHandler handler = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        handler = s_handler;
        context = s_handler_context;
    }
    if (handler == nullptr) {
        ESP_LOGW(kTag, "Power key press with no handler attached");
        return;
    }

    ESP_LOGI(kTag, "Power key %s press", press == Press::kLong ? "long" : "short");
    handler(press, context);
}

}  // namespace

void SetPressHandler(PressHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_handler = handler;
    s_handler_context = context;
}

esp_err_t Init()
{
    Axp2101* pmic = waveshare_board::GetPmic();
    if (pmic == nullptr) {
        ESP_LOGE(kTag, "PMIC unavailable; power key is inert");
        return ESP_ERR_INVALID_STATE;
    }

    pmic->SetInterruptCallback(&HandleInterrupt);
    ESP_LOGI(kTag, "Power key handler attached");
    return ESP_OK;
}

}  // namespace power_key_runtime
