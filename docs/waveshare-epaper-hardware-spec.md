# esp-epaper Hardware Specification (Software Porting Edition)

> This document is intended for firmware/software porting of the `followup`
> firmware on its retained `esp-epaper` board target. It focuses on the main
> controller, key ICs, I2C addresses, power control, and ESP32-S3 GPIO
> definitions.
>
> The underlying board is the Waveshare **ESP32-S3-ePaper-3.97**. Vendor wiki:
> <https://docs.waveshare.com/ESP32-S3-ePaper-3.97>. Where the vendor wiki and
> the firmware disagree, the firmware is authoritative for what the running code
> actually drives — see [Software Porting Notes](#9-software-porting-notes).

## 1. System Overview

| Item | Specification |
|---|---|
| Board id | `esp-epaper` (Waveshare ESP32-S3-ePaper-3.97) |
| Main controller | ESP32-S3-WROOM-1-N16R8 (`esp32s3` target) |
| Flash / PSRAM | 16 MB flash / 8 MB octal PSRAM |
| Wireless | 2.4 GHz Wi-Fi (802.11 b/g/n) + Bluetooth 5 LE |
| Display | Waveshare 3.97" e-paper panel, `SSD1677` controller, `800x480` |
| Display bus | SPI (`SPI3_HOST`), e-paper control signals |
| Audio codec | ES8311 over I2S + shared I2C |
| Audio amplifier | NS4150B (enabled via `GPIO39`) |
| Board PCM format | `24 kHz`, mono, 16-bit |
| Storage expansion | MicroSD via `SDMMC 4-bit` mode |
| Power management | AXP2101 PMIC (wiki labels it `TG28`) |
| RTC | PCF85063 |
| IMU | QMI8658 (6-axis) |
| Temp/humidity | SHTC3 (present on board; not driven by current firmware) |
| USB | Native `USB-OTG` over the board USB-C connector |
| Shared control bus | I2C on `GPIO41` (SDA) / `GPIO42` (SCL) |

## 2. Key IC Part Numbers

| Function | Part Number | I2C Address | Software-Relevant Notes |
|---|---|---|---|
| Main controller | ESP32-S3-WROOM-1-N16R8 | — | ESP-IDF target `esp32s3`; 16 MB flash / 8 MB PSRAM |
| E-paper controller | SSD1677 | — | 800x480 panel; SPI command interface |
| Audio codec | ES8311 | `0x30` | I2S audio + I2C control; PA enable on `GPIO39` |
| Audio amplifier | NS4150B | — | Speaker power amp; enable driven by codec PA pin `GPIO39` |
| Power management IC | AXP2101 | `0x34` | Battery charging + system power rails + VBUS events |
| RTC | PCF85063 | `0x51` | Real-time clock; interrupt on `GPIO45` |
| 6-axis IMU | QMI8658 | `0x6B` / `0x6A` | Default `0x6B`, alternate `0x6A` (auto-tried); INT2 on `GPIO40` |
| Temp/humidity sensor | SHTC3 | `0x70` | On shared I2C bus; no driver in current firmware |
| MicroSD | — | — | `SDMMC 4-bit` mode socket |

## 3. I2C Peripheral Addresses

All I2C peripherals share the same master bus (`GPIO41` SDA / `GPIO42` SCL).

| Peripheral | Part Number | Address | Notes |
|---|---|---|---|
| Audio codec | ES8311 | `0x30` | `ES8311_CODEC_DEFAULT_ADDR` |
| PMIC | AXP2101 | `0x34` | `AXP2101_SLAVE_ADDRESS` |
| RTC | PCF85063 | `0x51` | `Pcf85063::kDefaultAddress` |
| 6-axis IMU | QMI8658 | `0x6B` / `0x6A` | Firmware tries `0x6B` first, then `0x6A` |
| Temp/humidity sensor | SHTC3 | `0x70` | On the bus per board design; not accessed by current firmware |

## 4. ESP32-S3 GPIO Definitions

### 4.1 E-paper Display (SPI3)

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `EPD_DC` | GPIO9 | Output | E-paper D/C select |
| `EPD_CS` | GPIO10 | Output | E-paper SPI CS |
| `EPD_SCK` | GPIO11 | Output | E-paper SPI CLK |
| `EPD_MOSI` | GPIO12 | Output | E-paper SPI MOSI (DIN) |
| `EPD_RST` | GPIO46 | Output | E-paper reset |
| `EPD_BUSY` | GPIO3 | Input | E-paper busy status |

### 4.2 Audio (I2S + Codec)

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `I2S_MCLK` | GPIO13 | Output | Codec master clock |
| `I2S_BCLK` | GPIO14 | Output | Codec bit clock |
| `I2S_WS` (LRCK) | GPIO47 | Output | Codec word select / LR clock |
| `I2S_DOUT` | GPIO48 | Output | Codec data out (playback) |
| `I2S_DIN` | GPIO21 | Input | Codec data in (mic capture) |
| `CODEC_PA` | GPIO39 | Output | Codec power-amplifier enable |
| `CODEC_I2C_SDA` | GPIO41 | I/O | Codec control I2C SDA (shared bus) |
| `CODEC_I2C_SCL` | GPIO42 | Output | Codec control I2C SCL (shared bus) |

### 4.3 I2C And Interrupts

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `I2C_SDA` | GPIO41 | I/O/Open-drain | Shared I2C SDA (codec/PMIC/RTC/IMU) |
| `I2C_SCL` | GPIO42 | Output/Open-drain | Shared I2C SCL (codec/PMIC/RTC/IMU) |
| `PMIC_IRQ` | GPIO38 | Input | AXP2101 interrupt (incl. VBUS insert/remove) |
| `RTC_INT` | GPIO45 | Input | PCF85063 RTC interrupt |
| `QMI8658_INT2` | GPIO40 | Input | IMU interrupt 2 |

### 4.4 Buttons And Navigation

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `BOOT_BUTTON` | GPIO0 | Input | BOOT/strap pin, also the primary action button |
| `NAV_BUTTON_UP` | GPIO4 | Input | Navigation up |
| `NAV_BUTTON_FUNCTION` | GPIO5 | Input | Navigation function / middle key |
| `NAV_BUTTON_DOWN` | GPIO6 | Input | Navigation down |

### 4.5 MicroSD (SDMMC 4-bit)

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `SD_D0` | GPIO15 | I/O | SD data 0 |
| `SD_D1` | GPIO7 | I/O | SD data 1 |
| `SD_D2` | GPIO8 | I/O | SD data 2 |
| `SD_D3` | GPIO18 | I/O | SD data 3 |
| `SD_CLK` | GPIO16 | Output | SD clock |
| `SD_CMD` | GPIO17 | I/O | SD command |

## 5. Power and Charging (AXP2101)

The AXP2101 PMIC owns system power, battery charging, and USB VBUS state. The
firmware profile is configured in the `Pmic` constructor in
[`components/board_epaper/epaper_board.cc`](/Users/tieuvong/Development/followup/components/board_epaper/epaper_board.cc).

### 5.1 Power Rails

These are the PMIC outputs the firmware enables at bring-up:

| Rail | State | Voltage | Notes |
|---|---|---|---|
| `DC1` | enabled | `3300 mV` | Main system 3.3 V |
| `ALDO1` | enabled | `3300 mV` | 3.3 V peripheral LDO |
| `ALDO2` | enabled | `3300 mV` | 3.3 V peripheral LDO |
| `ALDO3` | enabled | `3300 mV` | 3.3 V peripheral LDO |
| Button/backup battery | charge enabled | `3000 mV` | Coin/backup cell charge (e.g. RTC backup) |
| System power-down | — | `2800 mV` | PMIC cuts system power below this |

> Per-rail **load assignment** (which LDO feeds the display, codec, SD, or RTC)
> is a schematic-level fact and is not declared in firmware — the code only sets
> voltages and enables. Confirm rail→peripheral mapping against the board
> schematic before repurposing any rail.

### 5.2 Charging And VBUS

| Parameter | Value |
|---|---|
| VBUS voltage limit | `4.36 V` |
| VBUS current limit | `900 mA` |
| Charge target voltage | `4.2 V` |
| Charge constant current | `400 mA` |
| Precharge current | `75 mA` |
| Termination current | `25 mA` |
| Thermal threshold | `80 °C` |
| System power-down voltage | `2800 mV` |
| Low-battery warn threshold | `10 %` |
| Low-battery shutdown threshold | `5 %` |

### 5.3 Shutdown Model

Shutdown model (hybrid):

- power on from the PMIC hardware key path
- firmware-confirmed shutdown before PMIC power-off for the normal in-app flow
- PMIC-enforced `6s` power-key long-press shutdown as a forced fallback

The PMIC interrupt line (`GPIO38`) also exposes USB cable insert/remove state,
used for OTG/storage-mode cable detection and automatic exit back to app-owned
SD-card mode.

## 6. Sleep and Wake-Up

The firmware uses **light sleep** (not deep sleep) for inactivity, driven by the
device sleep service. The wake path is implemented in
[`main/service_runtime/device_sleep_runtime.cc`](/Users/tieuvong/Development/followup/main/service_runtime/device_sleep_runtime.cc).

Entry sequence before `esp_light_sleep_start()`:

- suppress display refresh and put the e-paper panel to sleep (`SleepPanel()`)
- prepare board power interrupts for wake
- arm GPIO wake sources, then sleep

### 6.1 Wake Sources

| Signal | GPIO | Trigger | Purpose |
|---|---:|---|---|
| `PMIC_IRQ` (POWERKEY) | GPIO38 | Low level | Power-key press / PMIC event wake |
| `BOOT_BUTTON` | GPIO0 | Low level | Primary action button wake |

Both are armed with `gpio_wakeup_enable(..., GPIO_INTR_LOW_LEVEL)` +
`esp_sleep_enable_gpio_wakeup()`. On exit the firmware reads both pin levels to
attribute the wake cause and suppress the spurious press event that caused it,
then calls `gpio_wakeup_disable()` on both and restores normal power interrupts.

### 6.2 Notes

- This is **light sleep**, so it keeps RAM/peripheral state; there is no EXT1 /
  RTC-domain `esp_sleep_enable_ext1_wakeup_io()` deep-sleep path in this firmware.
- IMU wake-on-motion is available in the QMI8658 driver but is **disabled** in
  this board profile (`QMI8658_ENABLE_WAKE_ON_MOTION = false`), so motion is not
  a wake source.
- RTC alarm handling is a firmware timer poll that is disabled during sleep — the
  RTC (`GPIO45`) is not used as a light-sleep wake GPIO.

## 7. Strapping Pins

The ESP32-S3 strapping pins carry startup risk; several are reused as functional
signals on this board and must keep a Boot-safe default level at reset:

| GPIO | Strap role | Board use | Caution |
|---|---:|---|---|
| `GPIO0` | Boot / download select | Primary action button (also POWERKEY wake) | Must be high at reset for normal boot |
| `GPIO3` | JTAG source select | `EPD_BUSY` input | Keep default level Boot-safe at startup |
| `GPIO45` | VDD_SPI voltage | `RTC_INT` input | Avoid asserting during reset |
| `GPIO46` | Boot / ROM messaging | `EPD_RST` output | Avoid asserting during reset |

## 8. USB / OTG Storage

- OTG mode uses the ESP32-S3 native USB device path to expose the SD card as
  USB mass storage
- the user enters USB storage from the Settings page via the `Enable OTG` action
- while active, the SD card is host-owned and the app blocks normal navigation
  behind a storage modal
- short power-key press, BOOT key activation, or USB cable removal requests exit
  back to app-mounted SD-card mode

## 9. Software Porting Notes

| Item | Notes |
|---|---|
| Source of truth | Pin values come from `epaper_board_config.h`; do not treat `docs/hardware-reference.md` audio pins as current — that file lists an older I2S mapping (`WS`/`DOUT`/`DIN` on GP15/GP16/GP21). The config header uses `WS=GPIO47`, `DOUT=GPIO48`, `DIN=GPIO21`, `MCLK=GPIO13`, `BCLK=GPIO14`. |
| Shared I2C bus | Codec, PMIC, RTC, and IMU share one master bus on `GPIO41`/`GPIO42`. |
| IMU address | QMI8658 auto-detects: try `0x6B` first, fall back to `0x6A`. |
| Strapping pins | `GPIO0/3/45/46` are strapping pins reused as functional signals — see [Strapping Pins](#7-strapping-pins). |
| SD mode | `SDMMC 4-bit` (not SPI); all four data lines connected (`D0`–`D3`). |
| Audio sample rate | Codec runs at `24 kHz` mono/16-bit; Gemini uplink path downsamples mic audio to `16 kHz` in software. |
| PMIC naming | The Waveshare wiki lists the PMIC as `TG28`; the firmware driver targets an AXP2101-compatible PMIC at I2C `0x34` and that is what actually works. Treat `0x34` / AXP2101 as authoritative. |
| SHTC3 sensor | Present on the board (shared I2C, `0x70`) but the current firmware ships no SHTC3 driver — add one before relying on temp/humidity. |
| Connectors | Battery, speaker, and RTC-backup-battery use MX1.25 headers (per vendor wiki); USB-C is used for flashing/logging and native USB-OTG. |

## 10. Build And Flash

```bash
source $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Release helper:

```bash
python3 scripts/release.py esp-epaper
```
