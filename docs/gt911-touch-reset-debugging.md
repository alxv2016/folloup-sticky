# GT911 Touch Reset Debugging

## Summary

The GT911 touch controller was reachable on I2C, but touch input did not work in the current build. The cached build reported the correct touch resolution and produced touch points, while the current build either reported `2048x2048`, `800x480`, or fallback values and then never emitted real touches.

The root cause was an incomplete Goodix reset sequence. After selecting the I2C address with the INT pin during reset, the driver released INT directly to input. The GT911 also needs an INT sync pulse after reset: drive INT low briefly, then release it back to input. Without that pulse, the controller could answer basic I2C reads such as product ID, but it did not reliably load/report its resolution or enter normal scanning mode.

## Symptoms

The failing current build showed logs like:

```text
GT911 begin: success
GT911 resolution: max_x=2048, max_y=2048
status read: addr=0x14 status=0x00 ready=0 count=0 int=1
```

After extra debugging, the chip was clearly alive on I2C:

```text
reset: id read ok addr=0x14 id='911' raw=39 31 31 00
resolution register: raw=00 00 00 00 sensor=2048x2048
status read: addr=0x14 touch=0x00 ready=0 count=0 int=1 request=0x00 ctrl=0x00
```

The cached working build showed the expected behavior:

```text
GT911 resolution: max_x=480, max_y=800
Touch point 0: x=263, y=276, size=19
```

## What We Checked

We compared the current build against the cached working build and narrowed the issue down in stages:

1. Checked whether the GT911 driver differed from the cached build.
2. Searched for GT911 config files or config blobs.
3. Looked for writes to the GT911 config area around `0x8047`, checksum at `0x80FF`, and `Config_Fresh` at `0x8100`.
4. Added debug reads for the resolution registers, status register, INT pin level, request register, and controller status register.
5. Disabled unrelated peripherals and tested touch-only.
6. Removed the e-paper driver path and tested only the touch controller.
7. Compared behavior against the upstream Linux Goodix driver reset flow.

The unrelated peripherals were ruled out because the issue still reproduced with a touch-only application.

## Debugging Timeline

### 1. Compared Current Logs Against Cached Working Logs

The cached working build reported:

```text
GT911 begin: success
GT911 resolution: max_x=480, max_y=800
Touch point 0: x=263, y=276, size=19
```

The current build originally reported bad or unstable resolution values:

```text
GT911 resolution: max_x=2048, max_y=2048
```

or:

```text
GT911 resolution: max_x=800, max_y=480
```

The important difference was not the e-paper or peripheral sequence. It was that the cached build entered a state where the GT911 reported valid dimensions and touch points, while the current build only reached basic I2C communication.

### 2. Compared the GT911 Driver Sources

We inspected the current GT911 driver and compared it against the cached/vendor driver path. The key register usage matched:

```text
0x8140 - product ID
0x8146 - resolution
0x814E - touch status
0x8150 - touch points
```

This suggested the driver was not obviously reading the wrong registers.

### 3. Looked for GT911 Config Files and Blobs

We searched the repo, cached build output, and nearby source trees for GT911 config data:

```text
0x8047
0x80FF
0x8100
Config_Fresh
GT911 config
Goodix config
```

We did not find a GT911 config blob in the source tree or cached firmware strings. That made it unlikely that the cached build was working because it uploaded a hidden config block.

### 4. Checked Whether the Resolution Register Was Really Empty

We added debug logging around the resolution register. The failing state read:

```text
resolution register: raw=00 00 00 00 sensor=2048x2048
```

That clarified that `2048x2048` was our driver fallback, not a real panel value. The GT911 was returning zeroes for resolution.

### 5. Checked Whether INT Was Being Held by the MCU

Because the status logs showed `int=1` forever, we suspected that the MCU might still be driving the GT911 INT pin and preventing the chip from asserting interrupts.

We explicitly released INT after initialization:

```cpp
gpio_set_pull_mode(static_cast<gpio_num_t>(PIN_TOUCH_INT), GPIO_FLOATING);
gpio_set_direction(static_cast<gpio_num_t>(PIN_TOUCH_INT), GPIO_MODE_INPUT);
```

The log confirmed the pin was released:

```text
GT911 INT released: level=1
```

Touch still did not work, so the problem was not simply that the MCU was holding INT high.

### 6. Added Product ID and Register Dumps

We dumped product ID and nearby registers before and after reset. The key result was:

```text
reset: id read ok addr=0x14 id='911' raw=39 31 31 00
debug reg 0x8047: 00 00 00 00 ...
debug reg 0x8140: 39 31 31 00 ...
```

This proved the chip was alive enough to answer ID reads, but the config/resolution region still looked empty.

### 7. Tested Longer Power-On and Hard Power Cycle

We tried increasing the power-on delay and then added a hard power cycle:

```text
TOUCH_EN low
RST low
INT held for address select
delay
TOUCH_EN high
delay
begin GT911
```

This did not fix the issue by itself. The first probe often still failed, the second probe succeeded, and the chip still returned zero resolution.

### 8. Ruled Out Other Peripheral Drivers

We disabled the non-touch demos:

```text
buzzer
buttons
charger
RTC
battery gauge
e-paper
```

Then we tested touch-only. The failure still reproduced:

```text
resolution register: raw=00 00 00 00 sensor=2048x2048
status read: addr=0x14 touch=0x00 ready=0 count=0 int=1
```

This ruled out the BQ27220, RTC, button, buzzer, and e-paper drivers as direct causes.

### 9. Checked the Goodix Request Register

The Linux Goodix driver handles a request register at `0x8043`. If the controller asks for config or main-clock data, a host driver may need to respond.

We added logging for:

```text
0x8043 - request
0x8044 - controller status
```

The logs showed:

```text
request=0x00 ctrl=0x00
```

That ruled out the theory that the chip was waiting for a config upload request.

### 10. Compared Reset Flow Against Linux Goodix Driver

The next difference was reset sequencing. The Goodix-style flow includes an INT sync step after releasing reset:

```text
RST low
INT selects address
RST high
INT low for about 50ms
INT released to input
```

Our driver was missing the post-reset INT-low sync pulse.

### 11. Added the INT Sync Pulse

We patched the reset routine to drive INT low for 50ms after RST goes high, then release INT:

```cpp
gpio_set_level(static_cast<gpio_num_t>(rstPin_), 1);
vTaskDelay(pdMS_TO_TICKS(kResetReleaseHighMs));

gpio_set_level(static_cast<gpio_num_t>(intPin_), 0);
vTaskDelay(pdMS_TO_TICKS(kIntSyncLowMs));

gpio_set_direction(static_cast<gpio_num_t>(intPin_), GPIO_MODE_INPUT);
vTaskDelay(pdMS_TO_TICKS(kResetAddressSettleMs));
```

After this change, the same hardware immediately reported the correct resolution and touch points:

```text
resolution register: raw=E0 01 20 03 sensor=480x800
touch=0x81 ready=1 count=1
point[0]: raw{x=134 y=412 size=32 id=0}
```

This confirmed the root cause.

## Important Findings

The GT911 product ID was readable:

```text
raw=39 31 31 00
```

That means I2C communication was not completely broken.

The resolution registers were initially all zero:

```text
raw=00 00 00 00
```

In our driver, that caused the sensor limits to remain at the default `2048x2048`. The `2048x2048` value was not a valid panel resolution; it was a fallback symptom.

The request register was not asking for config:

```text
request=0x00 ctrl=0x00
```

So the controller was not waiting for a host-side config upload through the Goodix request mechanism.

The INT pin was being released, but the controller was not entering normal scan behavior until the reset sequence was corrected.

## Root Cause

The reset sequence was missing the Goodix INT sync step.

Before the fix, the reset flow was effectively:

```text
RST low
INT set high/low to choose I2C address
RST high
INT released to input
```

The working Goodix-style flow needs an additional sync pulse:

```text
RST low
INT set high/low to choose I2C address
RST high
INT driven low for about 50ms
INT released to input
```

Without this post-reset INT low pulse, the GT911 could respond to ID reads but would not reliably report a valid resolution or touch data.

## Fix

The GT911 reset routine was updated to drive INT low for 50ms after RST is released, then switch INT back to input.

The important change is in `components/gt911/gt911.cpp`:

```cpp
gpio_set_level(static_cast<gpio_num_t>(rstPin_), 1);
vTaskDelay(pdMS_TO_TICKS(kResetReleaseHighMs));

gpio_set_level(static_cast<gpio_num_t>(intPin_), 0);
vTaskDelay(pdMS_TO_TICKS(kIntSyncLowMs));

gpio_set_direction(static_cast<gpio_num_t>(intPin_), GPIO_MODE_INPUT);
vTaskDelay(pdMS_TO_TICKS(kResetAddressSettleMs));
```

Where:

```cpp
constexpr uint32_t kIntSyncLowMs = 50;
```

This mirrors the reset/int-sync pattern used by Goodix drivers: after reset, pulse INT low, then release the interrupt pin.

## Result

After adding the INT sync pulse, the GT911 initialized correctly:

```text
resolution register: raw=E0 01 20 03 sensor=480x800
GT911 resolution: max_x=480, max_y=800
touch=0x81 ready=1 count=1
point[0]: raw{x=134 y=412 size=32 id=0}
Touch point 0: x=134, y=412, size=32
```

`E0 01 20 03` decodes as:

```text
0x01E0 = 480
0x0320 = 800
```

So the panel resolution is now read correctly, and touch points are reported.

## Follow-Up Cleanup

During debugging, `touch=0x80 count=0` appeared frequently:

```text
touch=0x80 ready=1 count=0
```

This is a ready/no-contact or finger-up style report, not a hard error. The driver was updated to clear the status and return quietly instead of logging it as an invalid touch count.

## Lessons

For GT911 bring-up, do not treat a readable product ID as proof that the controller is fully initialized. The chip can answer ID reads while still not scanning correctly.

The reset sequence matters:

```text
address select with INT during reset
release RST
sync INT low
release INT to input
then read ID/resolution/status
```

If resolution reads as all zeroes and touch status stays idle, verify the reset and INT sync sequence before chasing config blobs, unrelated peripherals, or display driver interactions.
