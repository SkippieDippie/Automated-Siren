# Hardware

## Controller

**STM32F103C8T6 "Blue Pill"** with an 8 MHz crystal, running at 72 MHz.
If the crystal fails to start, the firmware falls back to the internal
oscillator (64 MHz) and still works.

The firmware uses ~18 KB flash and ~5.5 KB RAM, so it fits the 64 KB
F103C8 as well as the F103CB.

## Pin map

All pins are defined in [`firmware/core/board.h`](../firmware/core/board.h).

| Function            | Pin   | Notes                                     |
|---------------------|-------|-------------------------------------------|
| MANUAL button       | PA0   | To GND, internal pull-up                  |
| STOP button         | PA1   | To GND, internal pull-up                  |
| Program 1 button    | PB12  | To GND, internal pull-up                  |
| Program 2 button    | PB13  | To GND, internal pull-up                  |
| Program 3 button    | PB14  | To GND, internal pull-up                  |
| Program 4 button    | PB15  | To GND, internal pull-up                  |
| Siren output        | PB1   | Active high → relay/SSR driver (see below) |
| Status LED          | PC13  | On-board LED, lit while the siren is powered |
| SD card SCK         | PA5   | SPI1                                      |
| SD card MISO        | PA6   | SPI1                                      |
| SD card MOSI        | PA7   | SPI1                                      |
| SD card CS          | PA4   |                                           |
| LCD SCL             | PB6   | I2C1, 100 kHz                             |
| LCD SDA             | PB7   | I2C1                                      |
| SWDIO / SWCLK       | PA13 / PA14 | Programming / debugging (ST-Link)   |

## LCD 1602

A standard 16×2 HD44780 display with a **PCF8574 I2C backpack**. The
firmware tries address `0x27` (PCF8574) and then `0x3F` (PCF8574A). Most
backpacks are 5 V; the STM32's PB6/PB7 are 5 V tolerant, and the
backpack's own pull-ups (to 5 V) are fine. Adjust the contrast trimmer on
the backpack if the screen looks blank.

## SD card

Use a **3.3 V SD card module** (or a module with a level shifter and
regulator, e.g. the common "Micro SD Card Adapter" with an AMS1117). Do not
connect a bare card to 5 V. Cards must be formatted **FAT16 or FAT32**;
SDHC cards up to 32 GB are fine. exFAT (the default for SDXC, >32 GB) is not
supported.

## Siren power stage

The STM32 pin only provides a 3.3 V logic signal, so it must drive the
siren motor through a suitable power stage. Typical options:

* **Solid-state relay** (e.g. a DC-input/AC-output SSR rated for the motor's
  inrush current) — input driven directly from PB1 if the SSR accepts 3 V
  input, otherwise through a transistor.
* **Relay module / contactor** — PB1 → NPN transistor or opto-isolated relay
  module → relay coil → contactor for the motor. Fit a flyback diode across
  any coil you drive directly.

If your driver is active-low (common on cheap relay modules), set
`SIREN_ACTIVE_HIGH` to `0` in `board.h`.

**Safety behaviour built into the firmware**

* The siren output is set to *off* before any other initialisation.
* An independent watchdog (~2 s) resets the controller if the firmware
  hangs, which switches the siren off.
* Manual mode switches off after 2 minutes of continuous use (thermal
  protection, configurable in `firmware/app/app_config.h`); release the
  button to reset.

Mains wiring must be done by a qualified person, and an external
emergency stop that cuts motor power independently of the controller is
recommended.

## Wiring overview

```
                 +----------------------+
  MANUAL  ──o o──┤PA0                PB1├──► relay/SSR driver ──► siren motor
  STOP    ──o o──┤PA1               PC13├──► LED
  P1      ──o o──┤PB12               PB6├──► LCD SCL  (PCF8574 backpack)
  P2      ──o o──┤PB13               PB7├──► LCD SDA
  P3      ──o o──┤PB14               PA4├──► SD CS
  P4      ──o o──┤PB15               PA5├──► SD SCK
   (all buttons  │                   PA6├◄── SD MISO
    to GND)      │   STM32F103C8     PA7├──► SD MOSI
                 +----------------------+
```
