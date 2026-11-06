# Hardware bring-up

`siren-bringup` is a separate test firmware for checking a newly built
board one part at a time, before flashing the real siren firmware. It
reports on the LCD and on a serial log, so you can still see what is
happening when the LCD itself is the problem.

> **Siren safety:** the siren output is only energised on the SIREN page,
> only while MANUAL is held, and for at most 10 seconds at a time. For the
> first power-up, disconnect the motor power stage or test with just the
> relay/SSR (no motor) connected.

## What you need

* The board wired as described in [hardware.md](hardware.md).
* An ST-Link programmer.
* Optional but recommended: a **3.3 V USB-serial adapter** (CH340, CP2102,
  FT232…). Connect adapter **RX → PA9** and **GND → GND**, and open a
  terminal at **115200 baud, 8N1** (e.g. PuTTY, `screen /dev/ttyUSB0
  115200`, or the Arduino serial monitor).

## Building and flashing

`cmake --build build` builds both firmwares. CI also attaches
`siren-bringup.bin` to every run. Flash it like the main firmware:

```bash
st-flash --reset write build/siren-bringup.bin 0x08000000
```

When you're done, flash `siren-panel.bin` the same way.

## Test sequence

### 1. Power-up and clock

Within a second of reset, the serial log shows:

```
=== Siren panel bring-up ===
[MCU] Clock: 8 MHz crystal (HSE) x9, SYSCLK 72000000 Hz
[MCU] Reset cause: POWER
```

* `INTERNAL HSI - crystal did not start!` means the 8 MHz crystal is not
  oscillating. The board still works at 64 MHz, but check the crystal and
  its load capacitors.
* On the main firmware, a reset cause of `WDOG` means the watchdog fired,
  i.e. the firmware hung. Running the bring-up after such a reset will
  show it.

### 2. LCD

The LCD shows three test screens, each for 1.5–2 s:

1. **All 32 cells solid black.** This confirms every character position
   works. Adjust the contrast pot until the blocks are clearly visible.
2. `0123456789ABCDEF` / `abcdefghijklmnop`. Check that all characters are
   correct. Wrong or garbled characters usually mean two data lines
   (D4–D7) are swapped or one is not connected.
3. `SIREN BRING-UP` / `Hold STOP: page`.

| You see                              | Likely cause                                 |
|--------------------------------------|----------------------------------------------|
| Nothing at all                       | No power to the LCD, or contrast far off     |
| Top row black blocks, bottom row empty, never changes | LCD not initialised: check RS (PB10), E (PB11), D4–D7 (PB12–PB15), and that **RW is tied to GND** |
| Garbled characters                   | Data lines swapped/open, or a poor ground    |
| Text works but is very faint         | Contrast, or a 5 V LCD that needs more than 3.3 V logic (see hardware.md) |

### 3. SD card

Next, `SD CARD TEST...` is shown while the card is tested, and the serial
log lists every program file:

```
[SD] OK: FAT32, 7580 MB.
[SD]   PROG1.TXT  OK  "Air Raid"  4 steps, 3:00
[SD]   PROG2.TXT  OK  "All Clear"  1 steps, 1:00
[SD]   PROG5.TXT  missing
[SD]   PROG6.TXT  ERROR line 4: Bad value
[SD] 4 of 6 programs OK.
```

Programs are checked with exactly the same loader and parser as the real
firmware, so a file that passes here will run on the panel.

### 4. Pages

Once the tests are done, the firmware shows four pages. **Hold STOP for
1 second** to go to the next page. The LED blinks twice a second as a
heartbeat, except on the SIREN page.

#### Page 1: BUTTONS

```
BUTTONS      1/4
M.3.....
```

Line 2 shows the inputs of the first 74HC165, in the order
`M S 1 2 3 4 5 6` (MANUAL, STOP, PROG1–PROG6). A pressed button shows its
letter; a released one shows `.`. With more chips, `+n` shows how many
buttons on the other chips are pressed. Every change is also logged:

```
[BTN] levels=0xEF pressed: P3
```

Press every button once and check that the right position lights up.

| You see                                   | Likely cause                              |
|-------------------------------------------|-------------------------------------------|
| `FAULT!`                                  | All inputs read low: the QH→PA2 line is shorted to GND, or the 165 has no power |
| Nothing ever changes                      | SH/LD (PA0) or CLK (PA1) not connected, or QH not connected to PA2 (reads all-released) |
| Wrong button lights up                    | Inputs wired in a different order (see the table in hardware.md) |
| A position flickers without being pressed | That input is missing its 10 kΩ pull-up    |

#### Page 2: SIREN OUT

```
SIREN OUT    2/4
MANUAL: off
```

Hold **MANUAL** to energise the siren output (PB1). The LED lights and the
relay/SSR should switch. After 10 seconds the output switches off
(`LIMIT: release`); release MANUAL to try again. This is the only page
where the siren output can be switched on.

#### Page 3: SD CARD

```
SD CARD      3/4
FAT32 7.4G 6/6
```

Card type, size, and how many program files loaded successfully. The test
runs again each time you enter this page, and when you press **P1**, so
you can swap cards or edit files and re-check them. Details are in the
serial log.

#### Page 4: INFO

```
72MHz HSE    4/4
0:05:12 POWER
```

Clock source (`64MHz HSI!` means the crystal isn't running), uptime, and
the reason for the last reset.
