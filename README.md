# Siren Control Panel

Firmware for an STM32-based control panel for a motor-driven siren.

* **Manual control.** While the MANUAL button is held, the siren motor is
  powered: it spins up and sounds. When the button is released, the motor
  coasts down and the sound stops.
* **Automatic programs.** Timed ON/OFF sequences, with nested repeats and
  "forever" loops.
* **Programs on an SD card.** Plain-text files `PROG1.TXT`…`PROG4.TXT`
  that you can edit on any PC ([format](docs/program-format.md)).
* **Program buttons.** P1–P4 start the corresponding program. Pressing the
  same button again, or STOP, ends it.
* **LCD 1602.** Shows the running program, whether the siren is on, time
  left in the current step, and total time left.

```
+----------------+      +----------------+      +----------------+
|P1 Air Raid     |      |MANUAL CONTROL  |      |SIREN READY     |
|ON   0:05 T 2:47|      |SIREN ON        |      |SD OK  P1-P4    |
+----------------+      +----------------+      +----------------+
  program running          button held             idle
```

## Behaviour

| Action                            | Result                                         |
|-----------------------------------|------------------------------------------------|
| Hold MANUAL                       | Siren on while held. This overrides a running program, which keeps counting down in the background. |
| Hold MANUAL > 2 min               | Siren switches off (thermal protection). Release the button to reset. |
| Press P1–P4                       | Load `PROGn.TXT` from the SD card and start it, replacing any running program. |
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
  drivers/   LCD1602 (PCF8574 I2C), SD card over SPI, FatFs glue
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

Output: `build/siren-panel.elf`, `.bin` and `.hex`.
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
wrap-around), debouncing, manual override and timeout, and the LCD screens.

```bash
make -C tests
```
