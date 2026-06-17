# reTerminal Sticky Hardware Specification (Software Porting Edition)

> This document is intended for firmware/software porting. It focuses on the main controller, key ICs, peripheral interfaces, I2C addresses, power control, and GPIO definitions.

## 1. System Overview

| Item | Specification |
|---|---|
| Main controller | ESP32-S3R8 |
| PSRAM | 8 MB PSRAM integrated in the main controller |
| External Flash | 256 Mbit QSPI Flash |
| Wireless | ESP32-S3 2.4 GHz Wi-Fi / BLE, onboard 2.4 GHz antenna matching network |
| Display | E-paper FPC interface, SPI control signals |
| Touch | Touch panel FPC interface, I2C control signals |
| Storage expansion | MicroSD, SPI/SD signals connected to ESP32-S3 |
| Audio input | PDM digital microphone |
| Sensors | 6-axis IMU, temperature and humidity sensor, RTC, battery fuel gauge |
| Power supply | USB-C 5 V / battery input, with charger and Power Path |
| System power rails | VSYS, VDD_3V3, RTC_3V3, EP_3V3, TP_3V3, VDD_SD, MIC_PWR |

## 2. Key IC Part Numbers

| Function | Ref. | Part Number | Software-Relevant Notes |
|---|---:|---|---|
| Main controller | U10 | ESP32-S3R8 | ESP32-S3, 8 MB PSRAM |
| QSPI Flash | U17 | W25Q256JVEIQ | 256 Mbit QSPI Flash, with an alternative compatible part number in the schematic |
| QSPI Flash compatible option | U9 | ZB25Q256AC | Alternative/compatible footprint to U17; final population depends on the BOM |
| Battery charger / Power Path | U1 | BQ25616 | 4.2 V battery charging, USB/VBAT to VSYS Power Path |
| Battery fuel gauge | U5 | BQ27220 | I2C address `0x55` |
| 3.3 V Buck-Boost | U2 | TPS631000DRLR | Converts VSYS to VDD_3V3 |
| RTC LDO | U4 | SGM2040-3.3YUDH4G/TR | Outputs RTC_3V3, 250 mA |
| Power latch logic | U3 | 74AHC1G79GW | Single D flip-flop used for Power latch / software-held power-on |
| USB-to-UART | U11 | CH343P | USB debug/download UART |
| Load switch | U7/U8/U12/U13 | TPS22916CYFPR | Power switching for TP, EP, SD, MIC, and similar rails |
| 6-axis IMU | U15 | LSM6DS3TR-C | I2C address `0x6A` |
| RTC | U14 | PCF8563M/TR | I2C address `0x51`, 32.768 kHz crystal |
| Temperature and humidity sensor | U6 | SHT40-AD1B-R2 | I2C address `0x44` |
| PDM microphone | MIC1 | MSM261DDB020 | PDM clock/data |
| Buzzer | BUZ1 | FUET-5018 | PWM drive |
| MicroSD socket | J2 | ST-TF-003J | SD card socket |
| USB-C connector | USB1 | TYPE-C-31-M-14 | USB 2.0 + VBUS |
| Battery connector | J1 | STP120BW-031 | Battery connector |
| E-paper FPC | J3 | FPC 24P TOP contact | E-paper display connector |
| Touch panel FPC | J4 | ST-FPC-W052006-2H | Touch connector |

## 3. I2C Peripheral Addresses

| Bus/Purpose | Peripheral | Part Number | Address | Notes |
|---|---|---|---|---|
| BFG I2C | Battery fuel gauge | BQ27220 | `0x55` | Explicitly marked in the schematic |
| MISC I2C | 6-axis IMU | LSM6DS3TR-C | `0x6A` | Explicitly marked in the schematic |
| MISC I2C | RTC | PCF8563M/TR | `0x51` | Explicitly marked in the schematic |
| MISC I2C | Temperature and humidity sensor | SHT40-AD1B-R2 | `0x44` | Explicitly marked in the schematic |
| TP I2C | Touch panel | GT911 | `0x5D` / `0x14` | Auto-detect address determined by INT/RST pin reset sequence; confirmed by project code |

## 4. ESP32-S3 GPIO Definitions

### 4.1 E-paper / Touch panel

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `EP_PWR_EN` | GPIO47 | Output | E-paper power enable |
| `EP_BUSY` | GPIO18 | Input | E-paper busy status |
| `EP_RSTn` | GPIO17 | Output | E-paper reset, active low |
| `EP_DCn` | GPIO16 | Output | E-paper D/C select |
| `EP_CSn` | GPIO15 | Output | E-paper SPI CS, active low |
| `EP_SDI` | GPIO14 | Output | E-paper SPI MOSI |
| `EP_SCK` | GPIO13 | Output | E-paper SPI CLK |
| `TP_PWR_EN` | GPIO42 | Output | Touch panel power enable |
| `TP_INT` | GPIO21 | Input | Touch interrupt |
| `TP_RSTn` | GPIO41 | Output | Touch reset, active low |
| `TP_I2C_SCL` | GPIO2 | Output/Open-drain | Touch I2C SCL |
| `TP_I2C_SDA` | GPIO3 | I/O/Open-drain | Touch I2C SDA |

### 4.2 USB UART / Download Control

| Signal | GPIO / Connection | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `USB_TXD` | GPIO44 / U0RXD side | Input | CH343P to ESP32-S3 UART RX |
| `USB_RXD` | GPIO43 / U0TXD side | Output | ESP32-S3 to CH343P UART TX |
| `UART_BOOT` | GPIO0 | Input | Download/Boot strap control |
| `UART_REST` | CHIP_PU | Input | Reset control |
| `USB_DP` | USB D+ | I/O | USB 2.0 D+ |
| `USB_DN` | USB D- | I/O | USB 2.0 D- |
| `USBC_VBUS` | VBUS | Input/Power | USB-C VBUS detection/power input |

### 4.3 Power / Battery

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `PWR_BUTTON` | GPIO4 | Input | Power button input |
| `PWR_HOLD` | GPIO45 | Output | Power latch data input, U3 D |
| `PWR_LOCK` | GPIO46 | Output/Input | Power latch clock input, U3 CP |
| `BFG_INT` | GPIO7 | Input | Fuel gauge interrupt |
| `BFG_I2C_SCL` | GPIO0 | Output/Open-drain | Fuel gauge I2C SCL; shares I2C1 bus with MISC_I2C |
| `BFG_I2C_SDA` | GPIO1 | I/O/Open-drain | Fuel gauge I2C SDA |
| `EN_BAT_CHGn` | GPIO39 | Output | Charge enable, active-low naming |
| `CHARGE_STATE` | GPIO40 | Input | Charge state |

> Note: `GPIO0` is also an ESP32-S3 strapping/download-related pin. If `BFG_I2C_SCL` is ultimately confirmed to connect to GPIO0, avoid any peripheral behavior or pull-up/pull-down state that could affect Boot mode during startup.

### 4.4 Peripherals

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `6D_INTn` | GPIO7 | Input | IMU interrupt |
| `BUTTON_UP` | GPIO5 | Input | Up button |
| `BUTTON_DOWN` | GPIO6 | Input | Down button |
| `BUZZER_PWM` | GPIO48 | Output/PWM | Buzzer PWM |
| `PDM_CLK` | GPIO19 | Output | PDM microphone clock |
| `PDM_DATA` | GPIO20 | Input | PDM microphone data |
| `PDM_EN` | GPIO38 | Output | Microphone power/enable control |
| `BUTTON_RESET` | CHIP_PU | Input | Reset button |
| `MISC_I2C_SCL` | GPIO0 | Output/Open-drain | IMU/RTC/SHT40 I2C SCL; shares I2C1 bus with BFG I2C |
| `MISC_I2C_SDA` | GPIO1 | I/O/Open-drain | IMU/RTC/SHT40 I2C SDA; shares I2C1 bus with BFG I2C |

### 4.5 MicroSD

| Signal | GPIO | Direction (from controller perspective) | Purpose |
|---|---:|---|---|
| `PWR_IN_VOLT` | GPIO9 | Input/ADC | Input voltage detection |
| `SD_DETECT` | GPIO11 | Input | SD card detection |
| `SD_PWR_EN` | GPIO10 | Output | SD card power enable |
| `SD_CMD/MOSI` | GPIO12 | Output | MicroSD SPI MOSI/CMD |
| `SD_D3/CS` | GPIO8 | Output | MicroSD SPI CS/DAT3 |
| `SD_CLK/SCK` | GPIO13 | Output | MicroSD SPI CLK; shares pin with EP_SCK |
| `SD_D0/MISO` | GPIO12 | Input | MicroSD SPI MISO/DAT0 |

## 5. Power and Low-Power Related Information

| Power Rail | Source/Control | Description |
|---|---|---|
| `VIN_5V` | USB-C VBUS | USB input 5 V |
| `VBAT` | Battery connector | Single-cell lithium battery input |
| `VSYS` | BQ25616 Power Path | Main system power rail |
| `VDD_3V3` | TPS631000DRLR | Main 3.3 V power rail |
| `RTC_3V3` | SGM2040-3.3 | RTC/low-power related power rail |
| `FG_1V8` | BQ27220 related | Fuel gauge internal/auxiliary power rail marking |
| `VDD_SD` | TPS22916CYFPR | Power rail after MicroSD power switch |
| `EP_3V3` | TPS22916CYFPR | E-paper power rail |
| `TP_3V3` | TPS22916CYFPR | Touch panel power rail |
| `MIC_PWR` | TPS22916CYFPR + MOS | Microphone power rail |

Power latch logic:

| Signal/Device | Description |
|---|---|
| `74AHC1G79GW` | Single D flip-flop used for Power latch |
| `PWR_BUTTON` | User button triggers power-on |
| `PWR_HOLD` / `PWR_LOCK` | ESP32-S3 controls U3 D/CP for software shutdown |
| `VDD_3V3_ENn` | Related to 3.3 V power enable; not routed to a confirmed ESP32-S3 GPIO in this revision; page 5 ties it to `RTC_INTn` |

Power-off notes:

- `PWR_HOLD` is U3 D and `PWR_LOCK` is U3 CP. The latest Page 6 trace also
  identifies `PWR_HOLD` as Q2's gate, with Q2 feeding `PWR_EN` through D5, and
  U3 Q as the signal that drives Q7.
- Earlier firmware experiments treated shutdown as either a U3 D/CP latch
  release or a direct Q2 gate drive. The current hard-off attempt combines both:
  pulse `PWR_LOCK` while `PWR_HOLD=0` to latch U3 Q low and release Q7, then
  drive `PWR_HOLD=1` to try to turn Q2 off before falling back to soft-off.
  Confirm Q2 channel type and pin assignment from the LP0404N3T5G datasheet and
  KiCad netlist before treating this polarity as final.
- No separate firmware-controlled `VDD_3V3_ENn` GPIO has been identified in the
  current hierarchical sheet view, so hard power-off depends on the discrete
  latch and RTC interrupt path rather than a direct buck-boost enable override.
- Page 5 ties `VDD_3V3_ENn` to `RTC_INTn`, which is on the always-on RTC rail.
  If `RTC_INTn` is asserted low, it may keep or re-enable the main 3.3 V rail.
  Clear or disable RTC interrupt/alarm flags before revisiting true hard-off.
- D2 is an ungated diode path from `VIN_5V` to `PWR_EN`, so USB-C input can keep
  `PWR_EN` asserted independently of Q2/U3/Q7 state while USB is plugged in.
- Current firmware has not achieved real power-off through GPIO45/GPIO46 on the
  tested board. Tried sequences include a D-low/CP-pulse release, delayed release
  after the physical power button goes high, GPIO45/GPIO46 high-Z after release,
  holding both latch lines low, and the revised Q2-gate attempt of `PWR_HOLD`
  high with `PWR_LOCK` low. In all tested cases the ESP32 remained powered.
- The implemented fallback is soft-off: firmware attempts latch release, then
  enters ESP32 deep sleep with `PWR_BUTTON` / GPIO4 as the active-low wake source.
- Before revisiting true hard power-off, confirm the actual U3/Q7/PWR_EN/RTC_INTn
  netlist or obtain a known-good vendor shutdown sequence for this board
  revision.

## 6. RTC / Deep-sleep Wake-up Recommendations

The ESP32-S3 GPIO range available for RTC external wake-up is `GPIO0 ~ GPIO21`. In this design, the pins that are related to low power/wake-up and fall within the RTC GPIO range include:

| Signal | GPIO | Can Be Used as an RTC Wake-up Candidate | Notes |
|---|---:|---|---|
| `PWR_BUTTON` | GPIO4 | Yes | Recommended as a button wake-up candidate |
| `BFG_INT` | GPIO7 | Yes | Can be used as a fuel gauge event wake-up candidate; confirm the fuel gauge interrupt level |
| `BUTTON_UP` | GPIO5 | Yes | Can be used as a button wake-up candidate |
| `BUTTON_DOWN` | GPIO6 | Yes | Can be used as a button wake-up candidate |
| `EP_BUSY` | GPIO18 | Yes | Generally not recommended for system sleep wake-up unless required by the display workflow |
| `TP_INT` | GPIO21 | Yes | Can be used as a touch wake-up candidate |
| `TP_I2C_SCL/SDA` | GPIO2/GPIO3 | Yes | These are RTC GPIOs, but as an I2C bus they are not recommended as direct wake-up sources |

Software recommendations:

- For Deep-sleep multi-GPIO wake-up, prefer `esp_sleep_enable_ext1_wakeup_io()`.
- ESP32-S3 EXT1 supports `ESP_EXT1_WAKEUP_ANY_HIGH` and `ESP_EXT1_WAKEUP_ANY_LOW`.
- After wake-up, if an RTC GPIO must be restored to normal GPIO function, call `rtc_gpio_deinit(GPIO_NUM_x)`.
- `GPIO0/GPIO3/GPIO45/GPIO46` are pins with higher startup/strapping risk; firmware initialization and peripheral default levels should be handled carefully.

## 7. Software Porting Notes

| Item | Notes |
|---|---|
| `SHT40-AD1B-R2` address | Schematic I2C Tree page marks `0x44`, Peripherals page marks `0x45`; recommend `0x44` as primary, try `0x45` if communication fails |
| `MISC_I2C_SCL/SDA` GPIO | GPIO0 (SCL) / GPIO1 (SDA); shares I2C1 bus with BFG I2C |
| Touch panel IC | GT911, I2C address `0x5D` / `0x14` auto-detect (determined by INT/RST reset sequence) |
| MicroSD bus mode | SD_D1 / SD_D2 are not connected (NC); SD card uses SPI mode only (CLK/CMD/D0/D3) |
