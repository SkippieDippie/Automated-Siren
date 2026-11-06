# Siren Control Panel

[![CI](https://github.com/SkippieDippie/Automated-Siren/actions/workflows/ci.yml/badge.svg)](https://github.com/SkippieDippie/Automated-Siren/actions/workflows/ci.yml)

Firmware for an STM32-based control panel for a motor-driven siren.

* **Manual control.** While the MANUAL button is held, the siren motor is
  powered: it spins up and sounds. When the button is released, the motor
  coasts down and the sound stops.
* **Automatic programs.** Timed ON/OFF sequences, with nested repeats and
  "forever" loops.
* **Programs on an SD card.** Plain-text files `PROG1.TXT`, `PROG2.TXT`, …
  that you can edit on any PC ([format](docs/program-format.md)).
* **Program buttons.** Buttons are read through a 74HC165 shift register:
  one chip gives MANUAL, STOP and 6 program buttons, and up to 4 chips can be
  chained for 30 programs. Each program button starts its program. Pressing the
  same button again, or STOP, ends it.
* **LCD 1602**, driven directly in 4-bit mode (no I2C backpack). Shows the
  running program, whether the siren is on, time left in the current step,
  and total time left.

```
+----------------+      +----------------+      +----------------+
|P1 Air Raid     |      |MANUAL CONTROL  |      |SIREN READY     |
|ON   0:05 T 2:47|      |SIREN ON        |      |SD OK  P1-P6    |
+----------------+      +----------------+      +----------------+
  program running          button held             idle
```

## Behaviour

| Action                            | Result                                         |
|-----------------------------------|------------------------------------------------|
| Hold MANUAL                       | Siren on while held. This overrides a running program, which keeps counting down in the background. |
| Hold MANUAL > 2 min               | Siren switches off (thermal protection). Release the button to reset. |
| Press a program button (P1–P6)    | Load `PROGn.TXT` from the SD card and start it, replacing any running program. |
| Press the running program's button | Stop it.                                      |
| Press STOP                        | Stop the running program.                      |
| Program ends                      | `Pn DONE` is shown for 3 s.                    |

Behaviour settings are in [`firmware/app/app_config.h`](firmware/app/app_config.h),
and pins are in [`firmware/core/board.h`](firmware/core/board.h).
See [docs/hardware.md](docs/hardware.md) for wiring.

## Project layout

```
firmware/
  app/       Portable logic, no hardware access (unit-tested on PC)
             program.c  - program file parser
             player.c   - real-time sequencer
             buttons.c  - debouncing
             app.c      - control logic and LCD screens
  core/      STM32 entry point, clock/GPIO/watchdog, port implementation
  drivers/   LCD1602 (4-bit parallel), 74HC165 buttons, SD card over SPI,
             FatFs glue, serial log
  bringup/   Hardware test firmware (docs/bringup.md)
  config/    HAL and FatFs configuration
tests/       Host unit tests (fake hardware port)
sd-card/     Example program files: copy them to the card's root folder
third_party/ ST HAL, CMSIS, FatFs (git submodules)
```

## Building

Requirements: `arm-none-eabi-gcc`, CMake ≥ 3.20, and make or Ninja.

```bash
git submodule update --init --depth 1
```

```bash
cmake -S . -B build
```

```bash
cmake --build build
```

This builds two firmwares:

* `build/siren-panel.elf` / `.bin` / `.hex`: the siren control panel.
* `build/siren-bringup.elf` / `.bin` / `.hex`: a hardware test firmware
  for checking a newly built board (LCD, buttons, SD card, siren output)
  before running the real thing. See [docs/bringup.md](docs/bringup.md).
Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build; this also keeps the
watchdog from resetting the MCU while it is halted in the debugger.

## Flashing (ST-Link)

```bash
st-flash --reset write build/siren-panel.bin 0x08000000
```

Or with OpenOCD:

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/siren-panel.elf verify reset exit"
```

## Tests

The application logic runs on a PC against a fake hardware port. The tests
cover the parser, the sequencer timing (including late ticks and timer
wrap-around), debouncing, the shift-register fault check, manual override
and timeout, and the LCD screens.

```bash
make -C tests
```

## Continuous integration

GitHub Actions ([`.github/workflows/ci.yml`](.github/workflows/ci.yml))
runs on every push and pull request:

* the host unit tests, with AddressSanitizer and UndefinedBehaviorSanitizer;
* firmware builds in release, debug and 4-button-chip configurations, with
  warnings treated as errors.

The release build's `.elf`, `.bin`, `.hex` and `.map` files, plus the
bring-up firmware, are attached to each run as a downloadable artifact. Flash the `.bin` with the commands
above.
