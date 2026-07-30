#ifndef USB_STORAGE_BACKEND_H_
#define USB_STORAGE_BACKEND_H_

#include "esp_err.h"

// Exposes the SD card to a USB host as a mass-storage device (the board's "OTG" mode).
//
// This owns its own sdmmc card handle rather than reusing the app's mount: TinyUSB MSC
// needs raw block access, so the FATFS mount has to be released first. storage_service is
// what sequences that -- unmount app-side, enter here, and remount on the way out.
//
// While USB mode is active the app has no filesystem: no recording, no archive reads.
namespace usb_storage_backend {

esp_err_t EnterUsbMode();
esp_err_t ExitUsbMode();
bool IsActive();

}  // namespace usb_storage_backend

#endif  // USB_STORAGE_BACKEND_H_
