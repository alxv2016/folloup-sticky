#ifndef POWER_KEY_RUNTIME_H_
#define POWER_KEY_RUNTIME_H_

#include <cstdint>

#include "esp_err.h"

namespace power_key_runtime {

// The AXP2101 power key, decoded into the two presses the firmware acts on. A sustained
// 6s hold never reaches here: the PMIC cuts the rails itself, so it stays a hardware
// escape hatch independent of anything running.
enum class Press : uint8_t {
    kShort,
    kLong,
};

using PressHandler = void (*)(Press press, void* context);

// Product policy (what a press does) lives with the caller; this runtime owns only the
// PMIC plumbing and the decode.
void SetPressHandler(PressHandler handler, void* context);

// Attaches to the PMIC interrupt stream. The IRQ sources are enabled during rail bring-up
// in waveshare_board, so this only binds the handler -- call it once the UI surfaces a
// press can reach are initialized.
esp_err_t Init();

}  // namespace power_key_runtime

#endif  // POWER_KEY_RUNTIME_H_
