# Hardware

## Controller

**STM32F103C8T6 "Blue Pill"** with an 8 MHz crystal, running at 72 MHz.
If the crystal fails to start, the firmware falls back to the internal
oscillator (64 MHz) and still works.

The firmware uses ~17 KB flash and ~5.5 KB RAM, so it fits the 64 KB
F103C8 as well as the F103CB.

## Pin map

All pins are defined in [`firmware/core/board.h`](../firmware/core/board.h).

| Function             | STM32 pin | Connects to                          |
|----------------------|-----------|--------------------------------------|
| Buttons: load        | PA0       | 74HC165 pin 1 (SH/LD)                |
| Buttons: clock       | PA1       | 74HC165 pin 2 (CLK)                  |
| Buttons: data        | PA2       | 74HC165 pin 9 (QH)                   |
| LCD RS               | PB10      | LCD pin 4                            |
| LCD E                | PB11      | LCD pin 6                            |
| LCD D4               | PB12      | LCD pin 11                           |
| LCD D5               | PB13      | LCD pin 12                           |
| LCD D6               | PB14      | LCD pin 13                           |
| LCD D7               | PB15      | LCD pin 14                           |
| SD card CS           | PA4       |                                      |
| SD card SCK          | PA5       | SPI1                                 |
| SD card MISO         | PA6       | SPI1                                 |
| SD card MOSI         | PA7       | SPI1                                 |
| Siren output         | PB1       | Relay/SSR driver (see below)         |
| Status LED           | PC13      | On-board LED, lit while siren powered |
| SWDIO / SWCLK        | PA13 / PA14 | ST-Link (programming/debugging)   |
| Serial log TX        | PA9       | USB-serial adapter RX, 115200 baud (bring-up firmware only, optional) |

## Buttons: 74HC165 shift register

Buttons are read through a **74HC165** (parallel-in, serial-out) shift
register, so 8 buttons need only 3 STM32 pins, and more chips can be
chained for more buttons.

**Power the 74HC165 from 3.3 V**, not 5 V, so that its output is safe for
the STM32. The 74HC family runs at 2–6 V.

### Input assignment (one chip)

| 165 input | Pin | Button   |
|-----------|-----|----------|
| A         | 11  | MANUAL   |
| B         | 12  | STOP     |
| C         | 13  | PROG 1   |
| D         | 14  | PROG 2   |
| E         | 3   | PROG 3   |
| F         | 4   | PROG 4   |
| G         | 5   | PROG 5   |
| H         | 6   | PROG 6   |

Each input has a **10 kΩ pull-up to 3.3 V**, and its button connects the
input to **GND** (pressed = low). A 10 kΩ ×8 resistor network (SIP-9)
makes this tidy. Unused inputs must still be pulled up; a floating input
reads as random presses.

Other 74HC165 pins:

| Pin | Name    | Connection                                         |
|-----|---------|----------------------------------------------------|
| 1   | SH/LD   | STM32 PA0                                          |
| 2   | CLK     | STM32 PA1                                          |
| 7   | QH̅      | Not connected                                      |
| 8   | GND     | GND                                                |
| 9   | QH      | STM32 PA2                                          |
| 10  | SER     | GND for the last chip in the chain (otherwise QH of the next chip) |
| 15  | CLK INH | GND                                                |
| 16  | VCC     | 3.3 V, with a 100 nF capacitor to GND close to the chip |

### Expanding: more chips

Chain up to 4 chips (32 inputs: MANUAL, STOP and up to 30 programs):

* All chips share SH/LD (PA0) and CLK (PA1).
* Chip 0's QH goes to PA2. Chip 1's QH goes to chip 0's SER, chip 2's QH
  to chip 1's SER, and so on. The last chip's SER goes to GND.
* Chip 1 inputs A…H are PROG 7…PROG 14, chip 2 inputs are PROG 15…22, etc.

Then set `APP_BUTTON_CHIPS` in
[`firmware/app/app_config.h`](../firmware/app/app_config.h). Program buttons
read `PROG7.TXT`, `PROG8.TXT`, … in the same way.

### Wiring fault protection

If the data line is stuck low (shorted, chip unpowered or missing), every
button would read as pressed, including MANUAL, which would start the siren.
The firmware recognises "all inputs pressed at once" as a wiring fault and
treats it as *no* button pressed. A disconnected data line reads high
(internal pull-up), which also means "nothing pressed".

## LCD 1602 (parallel, 4-bit mode)

A standard 16×2 HD44780-compatible display driven directly by 6 STM32
pins.

| LCD pin | Name | Connection                                           |
|---------|------|------------------------------------------------------|
| 1       | VSS  | GND                                                  |
| 2       | VDD  | 5 V (or 3.3 V for a 3.3 V LCD module)                |
| 3       | V0   | Contrast: wiper of a 10 kΩ pot between VDD and GND   |
| 4       | RS   | PB10                                                 |
| 5       | RW   | **GND** (write-only, required)                       |
| 6       | E    | PB11                                                 |
| 7–10    | D0–D3 | Not connected                                       |
| 11      | D4   | PB12                                                 |
| 12      | D5   | PB13                                                 |
| 13      | D6   | PB14                                                 |
| 14      | D7   | PB15                                                 |
| 15      | A    | Backlight +: 5 V through a resistor (many modules have one fitted; check) |
| 16      | K    | Backlight −: GND                                     |

**Voltage levels:** RW is tied to GND, so the LCD only ever *receives*
data. The STM32's 3.3 V outputs never see 5 V from the display. A 5 V
HD44780 normally accepts 3.3 V as logic high (VIH ≈ 2.2 V), but a few
clones need more. If the display shows nothing but black blocks on the
top row after adjusting the contrast, use a 3.3 V LCD module or a level
shifter (e.g. 74HCT245 powered from 5 V).

Lines are rewritten only when their text changes, and a full line takes
under 1 ms.

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
  inrush current): drive its input directly from PB1 if the SSR accepts a
  3 V input, otherwise through a transistor.
* **Relay module / contactor:** PB1 → NPN transistor or opto-isolated relay
  module → relay coil → contactor for the motor. Fit a flyback diode across
  any coil you drive directly.

If your driver is active-low (common on cheap relay modules), set
`SIREN_ACTIVE_HIGH` to `0` in `board.h`.

**Safety behaviour built into the firmware**

* The siren output is set to *off* before any other initialisation.
* An independent watchdog (~2 s) resets the controller if the firmware
  hangs, which switches the siren off.
* Manual mode switches off after 2 minutes of continuous use (thermal
  protection, configurable in `firmware/app/app_config.h`). Release the
  button to reset it.
* A stuck button data line is treated as "nothing pressed" (see above).

Mains wiring must be done by a qualified person. An external emergency
stop that cuts motor power independently of the controller is
recommended.

## Wiring overview

```
                     3.3V
                      │ 10k x8
          ┌───────────┼────────────┐
 MANUAL ─o o─ GND     │            │
 STOP   ─o o─ GND   ┌─┴──────────┐ │       +-------------------+
 P1..P6 ─o o─ GND   │ A..H       │ │       │                   │
                    │  74HC165   │ │  PA0 ─┤SH/LD   STM32   PB1├──► relay/SSR ──► motor
          GND ─ SER │         QH ├─┼──PA2  │      F103C8   PC13├──► LED
      GND ─ CLK INH │   SH/LD CLK│ │  PA1 ─┤CLK                │
                    └─────┬───┬──┘ │       │   PB10..PB15      ├──► LCD RS, E, D4..D7
                          │   │            │   PA4..PA7        ├──► SD card (SPI1)
                         PA0 PA1           +-------------------+
                                          LCD RW ── GND
```
