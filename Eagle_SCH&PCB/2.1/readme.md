# CrowPanel ESP32 Display 4.3-inch V2.1 Product Hardware Driver Guide

| Document Property | Details |
|---|---|
| Document Version | V1.0 |
| Date | 2026-07-29 |
| Author | Codex (compiled through cross-referencing the project schematic and verified examples) |
| Applicable Hardware | CrowPanel ESP32 Display 4.3-inch V2.1 (schematic dated 2024-03-14) |
| MCU | ESP32-S3-WROOM-1-N4R2 |
| Software Baseline | Arduino-ESP32 / Arduino API; project `Arduino/Course` examples |

## 1. Document Purpose and Evaluation Criteria

This document is intended for hardware maintenance, driver porting, production testing, and onboarding handoffs. Information sources are evaluated according to the following priority:

1. Board-level example code in `Arduino/Course` that has been successfully run: used as the executable configuration baseline.
2. `2.1/CrowPanel ESP32 Display-4.3-inch-V2.1-20240314.sch`: used to verify components, nets, power supplies, and connectors.
3. Driver library source code provided with the project: used to identify library-internal parameters such as SPI mode and frequency.

If the code and schematic are inconsistent, the “Software Baseline” in the tables still uses the code values, while the inconsistency is recorded under “Differences/Risks.” Because the repository does not include test reports, a BOM, or hardware revision notes, “verified” specifically means that the repository provides course examples for this board; it does not mean that the board was retested as part of this effort. Label definitions:

- **A—Code and schematic match**: Can be used directly as the porting baseline.
- **B—Code takes precedence, with differences/resource sharing**: Operates according to the code, but risks must be addressed before porting.
- **C—Confirmed only by schematic**: The hardware is present, but the project contains no code that actively drives it.
- **D—External module example**: The interface has been verified by an example, but the module itself is not onboard.

## 2. Peripheral Overview

| Category | Component/Function | Reference Designator or Interface | MCU Resources | Software Layer | Status |
|---|---|---|---|---|---|
| MCU | ESP32-S3-WROOM-1-N4R2, 4 MB Flash / 2 MB PSRAM (based on the N4R2 model) | U2 | Entire system | Arduino-ESP32 (ESP-IDF/FreeRTOS underneath) | A |
| Display | 4.3-inch 480×272 RGB LCD, identified as NV3047 in code comments | JP1 | 16-bit RGB + DE/HSYNC/VSYNC/PCLK | Arduino_GFX 1.6.5, LVGL 9.1.0 | A |
| Backlight | LCD LED boost converter and switch | U4 MT9201 | GPIO2 | Arduino GPIO | A/B |
| Touch | Four-wire resistive touchscreen controller XP2046/XPT2046 | U41 | SPI: 11/12/13; CS 0; IRQ 36 | XPT2046_Touchscreen 1.4 | B |
| Storage | microSD/TF card | SD1 | SPI: 11/12/13; CS 10 | Arduino SPI, SD, FS | A |
| Audio | NS4168 I²S digital-input amplifier | U11 | BCLK 35, LRCLK 19, SDATA 20 | ESP32-audioI2S `Audio` | A |
| Speaker | 2-pin speaker output | J5 | Differential power output from U11 | No MCU driver | C |
| Wireless | 2.4 GHz Wi-Fi / BLE (integrated into MCU) | U2 | Internal RF | Arduino WiFi, ESP32 BLE | A |
| Debug/Programming | USB Type-C + CH340C USB-UART | J3, U6 | UART0 TXD0/RXD0; DTR/RTS automatic programming | Arduino `Serial` / ROM downloader | A/C |
| UART Expansion | UART1 / GPS example | J10, J4 | TX GPIO17, RX GPIO18 | `HardwareSerial(1)` | D |
| I²C Expansion | Crowtail/header I²C | J7, J4 | SCL GPIO38, SDA GPIO37 | Arduino `Wire` (this interface is not initialized by the code) | B/C |
| General-Purpose Expansion | 2.54 mm 8-pin combined interface | J4 | 3V3, GND, UART1, I²C | Select according to the use case | C |
| Buttons | BOOT, RESET | K1, K4 | GPIO0, EN | Hardware buttons/ROM boot | B/C |
| Indicator | Red power/enable indicator LED | LED1 | EN net through R3 | No GPIO control | C |
| Power Input | USB-C VBUS, lithium battery connector | J3, J1 | None | Hardware power path | C |
| Charge Management | Single-cell lithium battery linear charger | U26 4054A | No evidence of an MCU status interface | Hardware only | C |
| Main Power Supply | RY3420 DC/DC, PMOS power path | U1, Q3/Q30 | None | Hardware only | C |
| Backlight Power Supply | MT9201 boost converter | U4 | EN=GPIO2 | GPIO switch | A |

> The schematic does not show any independent onboard environmental or motion sensors, or actuators such as relays or motors. GPS is an external UART module example and should not be listed as an onboard component.

## 3. Consolidated Pin Assignment Table

| GPIO | Code Function | Schematic Net/Connection | Direction and Electrical Mode | Multiplexing/Notes |
|---:|---|---|---|---|
| 0 | XPT2046 CS | `IO0_BOOT` connected to touch CS through a 0 Ω configuration resistor; BOOT button/automatic programming | Push-pull output, active-low | Boot strapping pin; pulling it low during reset enters download mode |
| 1 | LCD B4 | `IO1_B7` | RGB peripheral push-pull output | Default UART0 TX-related capability should not be used concurrently |
| 2 | LCD backlight EN | `IO2_LCD_BL_CTR` → U4 EN | Push-pull output, active-high | The audio example also drives it high; it is not the audio amplifier enable |
| 3 | LCD B1 | `IO3_B4` | RGB output | Cannot be repurposed while dedicated to the display |
| 4 | LCD G5 | `IO4_G7` | RGB output | Same as above |
| 5/6/7 | LCD G0/G1/G2 | `IO5_G2`/`IO6_G3`/`IO7_G4` | RGB output | Same as above |
| 8/9 | LCD B0/B3 | `IO8_B3`/`IO9_B6` | RGB output | Same as above |
| 10 | TF CS | `IO10_TF_CS` → SD1 CS | Push-pull output, active-low | Shares SPI with touch; only CS is separate |
| 11 | SPI MOSI | `IO11_TF_SPI_MOSI/TP_DIN` | Push-pull output | Shared by TF and touch |
| 12 | SPI SCK | `IO12_TF_CLK/TP_CLK` | Push-pull output | Shared by TF and touch |
| 13 | SPI MISO | `IO13_TF_SPI_MISO/TP_OUT` | Input | Shared by TF and touch; unselected devices must be tri-stated |
| 14 | LCD R4 | `IO14_R7` | RGB output | Cannot be repurposed while dedicated to the display |
| 15/16 | LCD G3/G4 | `IO15_G5`/`IO16_G6` | RGB output | Same as above |
| 17 | UART1 TX | `IO17_TXD1` → J10/J4 | UART push-pull output, 3.3 V | GPS example uses 9600 baud |
| 18 | UART1 RX | `IO18_RXD1` → J10/J4 | UART input, 3.3 V | The external module’s TX must use 3.3 V logic |
| 19 | I²S LRCLK | Schematic name `IO19_I2S_MCLK`, connected through R12 to `LRCLK` | I²S output | Schematic net name “MCLK” is inconsistent with the actual code/amplifier LRCLK pin |
| 20 | I²S DATA | `IO20_I2S_SDIN`, connected through R4 to U11 SDATA | I²S output | The name SDIN is from the amplifier’s perspective |
| 21 | LCD R3 | `IO21_R6` | RGB output | Cannot be repurposed while dedicated to the display |
| 35 | I²S BCLK | `IO35_I2S_BCLK`, connected through R19 to U11 BCLK | I²S output | The schematic module symbol marks some GPIOs as NC; the net names/code are more reliable |
| 36 | Touch IRQ | `IO36_TP_IRQ` → U41 PENIRQ | Input, active-low/interrupt indication | The code polls the IRQ state with `tirqTouched()` and does not register an ISR |
| 37 | I²C SDA | `IO37_I2C_SDA` → J7/J4 | Open-drain, pull-up required | R7=4.7 kΩ is marked NC; do not assume the onboard pull-up is installed |
| 38 | Identified as LED in code; I²C SCL in schematic | `IO38_I2C_SCL` → J7/J4 | Configured by code as push-pull output; I²C requires open-drain | **Critical conflict**, see 5.1 |
| 39 | LCD HSYNC | `IO39_HSYNC` | RGB output | Configured as active-low |
| 40 | LCD DE | `IO40_DE` | RGB output | DE idle-low |
| 41 | LCD VSYNC | `IO41_VSYNC` | RGB output | Configured as active-low |
| 42 | LCD PCLK | `IO42_CLK_DCLK` | RGB output | 9 MHz, configured for active falling edge |
| 45/48/47 | LCD R0/R1/R2 | Identified as R3/R4/R5 in the schematic | RGB output | Defined by the software according to RGB565 bus bit order |
| 46/9/1 | LCD B2/B3/B4 | Identified as B5/B6/B7 in the schematic | RGB output | GPIO46 often has input/strapping restrictions; confirm for the target chip because it is currently driven by the RGB peripheral |

## 4. Detailed Peripheral Driver Guide

### 4.1 ESP32-S3 MCU, Flash, and PSRAM

- **Hardware**: U2 is explicitly identified as `ESP32-S3-WROOM-1-N4R2`. Flash and PSRAM are internal to the module and do not occupy any external GPIOs listed in this document.
- **Software layer**: Arduino-ESP32 is built on ESP-IDF and FreeRTOS; the examples use Arduino `setup()/loop()`, GPIO, SPI, UART, Wi-Fi, and BLE APIs without direct register access.
- **Porting configuration**: The target board must be configured as an ESP32-S3 and match the N4R2 model’s 4 MB Flash and 2 MB PSRAM partition/memory configuration. RGB frame buffers and LVGL are RAM-sensitive, so enabling PSRAM is recommended.
- **Boot/reset**: EN is an active-high reset net, and K4 pulls it low to reset the device. GPIO0 is a boot strapping pin that can be pulled low by K1 or the CH340C automatic programming circuit.

### 4.2 RGB LCD and LVGL

**Interface and Pins**

The LCD uses a 16-bit RGB565 data bus, not an SPI LCD. The data bits are assigned as follows:

```cpp
Arduino_ESP32RGBPanel bus(
  40, 41, 39, 42,                       // DE, VSYNC, HSYNC, PCLK
  45, 48, 47, 21, 14,                   // R0..R4
  5, 6, 7, 15, 16, 4,                   // G0..G5
  8, 3, 46, 9, 1);                      // B0..B4
```

**Key Timing Parameters**

| Parameter | Software Baseline |
|---|---:|
| Resolution | 480 × 272 |
| Pixel format | RGB565, 16 bit |
| PCLK | 9 MHz |
| HSYNC polarity | Low (0) |
| H front / pulse / back | 8 / 4 / 43 pixels |
| VSYNC polarity | Low (0) |
| V front / pulse / back | 8 / 4 / 12 lines |
| PCLK sampling configuration | `pclk_active_neg=1` |
| DE idle / PCLK idle | 0 / 0 |
| bounce buffer | 480 pixels |
| Display rotation | 0 |
| Automatic refresh | `false` |

**Initialization Sequence**

1. Configure GPIO2 as an output and drive it high to enable the backlight.
2. Call `lcd->begin()` to initialize the RGB peripheral and display object; stop execution if initialization fails.
3. Call `touch_init()` to initialize the touch controller on the shared SPI bus.
4. After `lv_init()`, use `millis` as the tick source and create a 480×272 display.
5. Use a local rendering buffer containing `480*272/8` elements of type `lv_color_t`, with `LV_DISPLAY_RENDER_MODE_PARTIAL`.
6. The flush callback calls `draw16bitRGBBitmap()`; the main loop calls `lv_timer_handler()` approximately every 16 ms.

**Dependencies**: GFX Library for Arduino 1.6.5 and LVGL 9.1.0. Code comments identify the panel as NV3047, but the code constructs a generic `Arduino_RGB_Display` and does not perform separate NV3047 command initialization. When replacing the panel, revalidate the porch values, polarities, and PCLK against the panel datasheet.

### 4.3 LCD Backlight

- **Hardware path**: GPIO2 → R11 (100 Ω) → U4 MT9201 EN; U4, L5, and D6 form the LED boost converter, with output `LCD_LEDA` and return terminal `LCD_LEDK`.
- **Drive method**: The current code uses a GPIO push-pull switch: `HIGH` turns it on and `LOW` turns it off.
- **Initialization**: Drive the pin high before calling `lcd->begin()`.
- **Dimming**: PWM is not currently used. If PWM is added, confirm the PWM frequency, minimum pulse width, and audible-noise characteristics supported by the U4 EN input; do not assume that an arbitrary `ledc` frequency is suitable.
- **Risk**: `LCD_LEDA` is the boosted LED supply, not a 3.3 V GPIO. Do not probe or short it to an MCU pin.

### 4.4 XPT2046 Resistive Touchscreen

| Item | Software Baseline |
|---|---|
| Controller | XP2046 (schematic) / XPT2046 (library, protocol-compatible) |
| SPI SCK/MOSI/MISO | GPIO12 / GPIO11 / GPIO13 |
| CS | GPIO0, active-low |
| PENIRQ | GPIO36, active-low indication |
| SPI parameters | 2 MHz, MSB first, SPI mode 0 (library `SPISettings`) |
| Rotation | 0 |
| Raw X calibration | 4000 → 100 |
| Raw Y calibration | 100 → 4000 |
| Read method | `tirqTouched()` checks the signal, and `touched()/getPoint()` samples it; no attachInterrupt ISR |

```cpp
SPI.begin(12, 13, 11, 0);
ts.begin();
ts.setRotation(0);
```

- SPI is shared with the TF card on GPIO11/12/13, with separate chip-select pins GPIO0/GPIO10. The CS pins of all devices not being accessed must remain high.
- GPIO0 is also the BOOT strapping pin. Using it as CS while the application is running is feasible, but the touch controller, resistor network, and button must not pull it low during the power-on/reset window, or the device will enter ROM download mode.
- In the schematic, GPIO36 appears as `NC2` on the module symbol, but the net is named `IO36_TP_IRQ`, and the code does use GPIO36. As required, the code value is used, while the symbol library error is recorded as a risk.
- **Known coordinate defect**: The LCD width is 480, but the touch code maps coordinates to 430/429; the height is 272. During porting, perform four-corner or nine-point calibration on the board and, after verification, standardize the output width to `480` or `480-1`. Do not merely change the macro without physical testing, because the raw 4000/100 range also varies between individual units.

### 4.5 microSD/TF Card

| Signal | GPIO | Description |
|---|---:|---|
| CS | 10 | Active-low, schematic net `IO10_TF_CS` |
| MOSI/DI | 11 | Shared with touch |
| SCK | 12 | Shared with touch |
| MISO/DO | 13 | Shared with touch |

Initialization baseline:

```cpp
SPI.begin(12, 13, 11);
SPI.setFrequency(1000000);   // 音频示例明确设置 1 MHz
if (!SD.begin(10)) { /* mount failed */ }
```

- The standalone SD example does not explicitly set the frequency, allowing the Arduino SD library to select its default value. The audio example explicitly reduces it to 1 MHz for stability. Therefore, 1 MHz is a conservative shared-bus baseline supported by code evidence, not the card’s maximum speed.
- SD1 DATA1/DATA2 have 10 kΩ bias resistors, but the software operates in SPI mode rather than 4-bit SDMMC mode.
- The audio example requires `/123.mp3` in the root directory and must call `audio.loop()` continuously.
- When used together with touch, each transaction must use `beginTransaction/endTransaction` correctly through the libraries, and both CS pins must never be low simultaneously.

### 4.6 I²S Audio Amplifier and Speaker

| Signal | GPIO | Schematic Path |
|---|---:|---|
| BCLK | 35 | Through R19 0 Ω → U11 BCLK |
| LRCLK/WS | 19 | `IO19_I2S_MCLK` through R12 0 Ω → U11 LRCLK |
| SDATA | 20 | `IO20_I2S_SDIN` through R4 0 Ω → U11 SDATA |

```cpp
audio.setPinout(35, 19, 20);
audio.setVolume(21);         // 库范围 0..21
audio.connecttoFS(SD, "/123.mp3");
```

- **Component**: U11 is an NS4168 digital-input amplifier, with the speaker connected to the differential output at J5. Neither speaker terminal may be connected to system GND.
- **Clocking**: The code configures only BCLK/LRCLK/DATA, with no separate MCLK. The schematic names the GPIO19 net `I2S_MCLK`, but it ultimately connects to the U11 `LRCLK` pin; follow the code and amplifier pin.
- **CTRL**: U11 CTRL is configured by external resistors/transistors in hardware. The project provides no evidence of independent MCU GPIO control.
- **Sampling parameters**: The example plays MP3 files, and the sample rate, bit depth, and channel configuration are dynamically set by the Audio library based on the file. The repository does not specify fixed values; do not hard-code an unverified fixed sample rate when porting.
- **Dependencies**: `Audio.h` (ESP32-audioI2S library), Arduino SD/FS/SPI. After successful initialization, `audio.loop()` must be called frequently to prevent underruns.
- **Difference**: The audio example subsequently drives GPIO2 high, which actually enables or maintains the LCD backlight rather than enabling the U11 amplifier. It may be removed in displayless applications, but the power path must be verified on the board first.

### 4.7 Wi-Fi and BLE

- Wi-Fi/BLE is integrated into the ESP32-S3 and requires no external digital driver pins. Keep the RF antenna area clear.
- The Wi-Fi example uses `WiFi.begin(ssid, password)`, with serial logging at 115200 baud. Credentials are application configuration and should not be hard-coded into handoff documentation.
- The BLE example creates a GATT Server named `ESP32SPI-BLE`, with Service UUID `6479571c-2e6d-4b34-abe9-c35116712345` and Characteristic UUID `826f072d-f87c-4ae6-a416-6ffdcaa02d73`.
- Software dependencies are Arduino WiFi and the ESP32 BLE stack, managed underneath by ESP-IDF/FreeRTOS. Wi-Fi and BLE share the 2.4 GHz radio, so throughput and latency must be retested under concurrent operation.

### 4.8 USB-C, CH340C, UART0, and Automatic Programming

- J3 USB-C D+/D− connect to CH340C U6 through 22 Ω series resistors; they do not connect directly to the ESP32-S3 native USB interface.
- CH340C TXD/RXD cross-connect to ESP32 UART0 RXD0/TXD0 through R71/R72 (22 Ω).
- DTR/RTS control EN and GPIO0 through Q9/Q10 to implement automatic reset and download mode.
- `Serial.begin(115200)` is the logging baseline for most examples; the audio example separately uses 9600. The physical output used by Arduino `Serial` also depends on the board-level USB CDC build options. For production programming, confirm that CH340 UART0 is actually selected.
- UART0 is the programming and logging channel and should not be repurposed as a product peripheral interface.

### 4.9 UART1 / External GPS Module

| Item | Configuration |
|---|---|
| UART controller | `HardwareSerial(1)` |
| Baud rate | 9600 |
| Format | 8 data bits, no parity, 1 stop bit (`SERIAL_8N1`) |
| MCU RX | GPIO18, connected to external module TX |
| MCU TX | GPIO17, connected to external module RX |
| Interface | J10 Crowtail, J4 header |

```cpp
HardwareSerial cardSerial(1);
cardSerial.begin(9600, SERIAL_8N1, 18, 17);
```

This example is a transparent serial bridge and does not parse NMEA; the GPS module is an external component. The interface is designed for 3.3 V logic levels. Do not connect it directly to a 5 V TTL output. J10 also provides power and GND; the peripheral’s peak current must be included in the power budget.

### 4.10 I²C Expansion Interface

- SCL=GPIO38 and SDA=GPIO37, routed to J7 and J4.
- I²C must use open-drain outputs. Schematic resistors R6/R7 are 4.7 kΩ but marked `/NC`, meaning they may not be installed. The external module must provide pull-ups suitable for the bus voltage, or the actual onboard assembly must be confirmed and supplemented as needed.
- The repository contains no verified `Wire.begin(37, 38)` example for this interface and specifies no onboard I²C slave address.
- All I²C configurations for FT6X36/GT911 in `touch.h` are commented out. They are part of a generic compatibility template and do not indicate that these devices are installed in this product.
- GPIO38 is configured as a push-pull output by the LED/LVGL example. Before using I²C, remove that LED control and reinitialize `Wire`.

### 4.11 BOOT, RESET, and Indicator LED

- K1: BOOT; pulls GPIO0 low when pressed and is shared with touch CS.
- K4: RESET; pulls EN low when pressed.- LED1: A red LED connected through R3=4.
7 kΩ to the EN net and then to GND, serving as a power-on/enable indicator. The schematic does not show it as being controlled by GPIO38.
- The `Example1_LED_blinking` course example and the LVGL UI name GPIO38 `LED` and toggle it high and low. According to the schematic, this toggles the I²C SCL/expansion port rather than LED1. When maintaining the code, rename this object to `GPIO38_TEST`, or update the schematic and revision history after confirming that a newer hardware revision does include an LED on GPIO38.

### 4.12 Power and Charging Management

**Power Path Overview**

- USB-C J3 supplies `VBUS`.
- J1 is the single-cell lithium battery connector on the `BAT+` net; U26 (4054A) handles linear charging.
- Q3, D2, and related components form the battery/external power path; U1 RY3420 and L4 form the main DC/DC converter, which outputs 3.3 V.
- Q30 routes the main power supply to `VIN2`; U4 boosts this power domain for the LCD backlight.
- The U11 amplifier is powered by the `VDD` power domain.

These circuits do not expose any identifiable MCU power-management or battery-level monitoring driver interface, so no software initialization is required. Note the following during maintenance:

- Use only a single-cell lithium battery system. Polarity, maximum charging current, and thermal design must be verified against the final BOM/component specifications.
- The 4054A programming resistor/actual charging current, RY3420 output capability, speaker power, and peak backlight consumption cannot be determined from the existing code alone and must not be guessed in production specifications.
- USB, charging, the backlight boost converter, and the audio amplifier can all generate heat/ripple. Perform system-level voltage-drop and temperature-rise testing with full backlight brightness, maximum volume, and wireless transmission active simultaneously.

## 5. Schematic–Code Discrepancy List

### 5.1 Conflicts That Must Be Addressed

| ID | Code Behavior | Schematic Evidence | Current Assessment | Maintenance Action |
|---|---|---|---|---|
| D-01 | GPIO38 is labeled as an LED and toggled using push-pull output | GPIO38=`IO38_I2C_SCL`, connected to J7/J4; LED1 is connected to EN | The code example name/target clearly does not match the V2.1 schematic | It may be used only for expansion-port testing when I²C is not connected; remove LED control before using I²C. Confirm whether an undocumented board revision exists |
| D-02 | Touch CS=GPIO0 | GPIO0 is also connected to BOOT, K1, and the automatic download circuit, and is routed through a configuration resistor to touch CS | Operational, but presents a boot risk | Ensure the touch CS circuitry does not pull the line low during reset sampling; investigate intermittent entry into download mode |
| D-03 | Touch output width is 430/429 | LCD=480×272 | Code defect or residual parameters from an older display | Perform nine-point calibration, then standardize the values to 480/479 |
| D-04 | Code identifies the controller as NV3047 | The schematic shows only a 40-pin RGB interface and does not specify the panel controller model | The part number cannot be confirmed from the code alone | Confirm the model using the physical display label/BOM; revalidate after any timing change |

### 5.2 Naming/Symbol Discrepancies

| ID | Discrepancy | Assessment |
|---|---|---|
| D-05 | Schematic net `IO19_I2S_MCLK` is actually connected through 0 Ω to U11 LRCLK, while the code uses it as LRC | Follow the code definition GPIO19=LRCLK; there is no independent MCLK |
| D-06 | U41 is labeled `XP2046`, while the library uses XPT2046 | The protocols are compatible; use XPT2046/XP2046 consistently in documentation |
| D-07 | Some GPIO36/35/37 pins in the ESP32 module symbol are labeled as NC | Give precedence to net names, the actual module model, and validated code; revise the schematic library |
| D-08 | The audio example drives GPIO2 high, but the comment can easily be interpreted as referring to the audio path | GPIO2 is actually the MT9201 backlight EN signal |
| D-09 | The schematic names the RGB nets as the panel’s upper bits R3..R7/G2..G7/B3..B7, while the code constructor parameters are named R0..R4, etc. | This is only local bit-order naming for the 16-bit bus; the physical GPIO mapping is consistent |

The exact causes of these discrepancies cannot be proven from the repository. Possible causes include incorrect schematic-library annotations, course examples migrated from a different display size, unsynchronized hardware revisions, or incorrect example naming. Maintenance records must distinguish between “confirmed causes” and “suspected causes.”

## 6. Resource Conflicts and Concurrent-Use Matrix

| Resource | Users | Concurrency Rules |
|---|---|---|
| SPI GPIO11/12/13 | TF card, XPT2046 | May be shared; GPIO10/GPIO0 are the respective chip-select lines; transaction parameters differ (touch is fixed at 2 MHz, mode 0, while the SD example conservatively uses 1 MHz) |
| GPIO0 | Touch CS, BOOT, automatic download | May be used as CS during runtime; peripherals must not pull it low during reset sampling |
| GPIO2 | Backlight EN | Must not be reused as a general-purpose output; driving it high in the audio example is equivalent to turning on the backlight |
| GPIO17/18 | UART1, J10, J4 | The same UART signals are wired in parallel to two connectors; do not connect two active driving sources simultaneously |
| GPIO37/38 | I²C, J7, J4, code “LED” | I²C and the GPIO38 LED example are mutually exclusive; the connectors are wired in parallel, so avoid excessively strong combined pull-ups |
| RGB GPIO 1/3–9/14–16/21/39–42/45–48 | LCD | These pins cannot be assigned to other peripherals while the display is enabled; pay particular attention to boot-strapping/input-restricted pins |
| UART0 | CH340C, downloading, logging | Product peripherals must not use this interface, or programming and diagnostics will be affected |
| 2.4 GHz RF | Wi-Fi, BLE | May coexist, but they share RF time slots; performance must be measured |

## 7. Recommended Overall Software Initialization Sequence

```text
Complete boot-strapping pin sampling
  → Serial logging
  → Drive all SPI CS lines high first (GPIO0, GPIO10)
  → Keep the backlight off initially (GPIO2=LOW)
  → Initialize the RGB LCD
  → Initialize the SPI bus and touch/TF devices (as needed)
  → Initialize LVGL and input devices
  → Initialize I²S audio, UART1, and I²C (according to product features)
  → Turn on the backlight after the first frame is stable
  → Continuously service LVGL / Audio / communication stacks in the main loop
```

The existing LVGL example turns on the backlight before LCD initialization. When porting to product firmware, it is recommended to turn it on only after the first frame has cleared the screen to reduce white-screen/corrupted-screen artifacts. This is an engineering recommendation and must be validated on the physical board after the change.

## 8. Porting Checklist

1. Select the Flash, PSRAM, and partition configuration corresponding to ESP32-S3-N4R2.
2. Pin Arduino_GFX 1.6.5, LVGL 9.1.0, and XPT2046_Touchscreen 1.4, or document the validation results for any upgrade.
3. Retain the RGB GPIO mapping and 9 MHz/porch parameters. First run solid-color and color-bar tests to verify bit order, polarity, and tearing.
4. Verify that GPIO0 does not incorrectly enter download mode under combinations of cold boot, warm reset, screen presses, and a connected programming cable.
5. Perform four-corner/nine-point touch calibration, resolve the 430-versus-480 width discrepancy, and verify rotation.
6. Mount the TF card and touch controller simultaneously and perform long-duration alternating access tests to confirm that CS handling and SPI transactions do not conflict.
7. Use TF cards with various capacities and speed classes to validate the 1 MHz baseline and the target higher-speed setting.
8. Audio testing must cover different sample rates, mono/stereo, maximum volume, concurrent wireless operation, and underrun recovery.
9. Before using I²C, confirm whether R6/R7 are populated, remove the GPIO38 LED logic, and measure the total pull-up resistance.
10. Confirm that UART1 peripherals use 3.3 V logic levels and crossed connections; do not connect two transmitters to J10/J4 simultaneously.
11. Validate CH340C automatic downloading, serial logging, and the production programming workflow.
12. Under both USB and battery power, test voltage drop, ripple, temperature rise, and resets with full backlight brightness, maximum volume, and Wi-Fi transmission active.

## 9. Key Risks/Precautions

- **GPIO38 conflict**: This is the clearest code–schematic conflict and may disrupt I²C communication or cause contention with external open-drain devices.
- **GPIO0 boot risk**: Touch CS is shared with BOOT; abnormal peripheral behavior may prevent the device from booting normally.
- **Incorrect touch coordinates**: 430/429 does not match 480, so the right side of the UI may be unreachable.
- **Shared SPI**: The TF card and touch controller must use independent chip-select lines and transactions; never assert both CS lines low simultaneously.
- **Uncertain I²C pull-ups**: The schematic marks the pull-ups as NC; inspect or measure the actual assembly.
- **Logic levels**: The MCU and digital expansion interfaces use 3.3 V logic; external 5 V TTL requires level shifting.
- **Heavy RGB pin usage**: Once the display is enabled, nearly all listed RGB GPIOs are unavailable for reuse.
- **High backlight voltage**: `LCD_LEDA` is a boosted-voltage node; during servicing and measurement, observe probe voltage ratings and short-circuit risks.
- **Differential speaker output**: Both J5 terminals are amplifier outputs and must not be connected to ground as a single-ended output.
- **Missing power data**: The available materials are insufficient to define a safe charging current, speaker power, or total power-consumption limit. Complete this information using the BOM, datasheets, and measurements before mass production.
- **Library version coupling**: Library upgrades may change RGB DMA, LVGL APIs, I²S, and SPI behavior; every upgrade requires full regression testing.

## 10. Maintenance and Acceptance Recommendations

The following items should be added later and placed under version control:

- Final BOM and LCD/touch part numbers and datasheets;
- Photos of the physical board revision and revision records;
- A single `board_pins.h` source of truth for pin definitions to prevent duplication and drift across examples;
- Automated pin-conflict checks, covering at least GPIO0, 2, 37, 38, and the shared SPI bus;
- NVS persistence for touch-calibration parameters;
- Production-test firmware covering RGB color bars, a 3×3 touch grid, TF read/write, audio frequency sweeps, Wi-Fi/BLE, UART loopback, buttons, and power stress testing;
- Measured results for each revision, including the highest stable TF frequency, display refresh rate, audio format matrix, temperature rise, and battery operating conditions.

## 11. Evidence Index

| Evidence | Purpose |
|---|---|
| `2.1/CrowPanel ESP32 Display-4.3-inch-V2.1-20240314.sch` | Component reference designators, nets, power supplies, connectors, and electrical paths |
| `2.1/CrowPanel ESP32 Display-4.3-inch-V2.1-20240314.pdf` | Source for manual schematic review |
| `Arduino/Course/LVGL_Arduino4.3/LVGL_Arduino4.3.ino` | RGB pins/timing, backlight, LVGL initialization, and GPIO38 behavior |
| `Arduino/Course/LVGL_Arduino4.3/touch.h` | Touch pins, calibration, rotation, and coordinate mapping |
| `Arduino/Course/Example2_Play_music/Example2_Play_music.ino` | I²S, TF frequency, volume, and playback flow |
| `Arduino/Course/Example3_SD_Card/Example3_SD_Card.ino` | TF mounting and capacity/directory checks |
| `Arduino/Course/Example5_BLE/Example5_BLE.ino` | BLE GATT parameters |
| `Arduino/Course/Example6_WIFI/Example6_WIFI.ino` | Wi-Fi STA initialization |
| `Arduino/Course/Example7_GPS_Module/Example7_GPS_Module.ino` | UART1 pins, 9600 8N1 |
| `Arduino/libraries/XPT2046_Touchscreen/XPT2046_Touchscreen.cpp` | Touch SPI at 2 MHz, MSB first, mode 0 |

---

**Handoff Conclusion**: Board-level examples provide a software baseline for the display, backlight, touch controller, TF card, I²S audio, wireless connectivity, and UART1; power/charging and USB-UART are primarily handled by hardware. The highest-priority porting tasks are not replacing driver libraries, but resolving the GPIO38/I²C conflict, GPIO0/BOOT multiplexing, and the 430→480 touch-coordinate defect, followed by regression testing on an actual V2.1 board.