# Siren program file format

Programs are plain-text files in the **root directory** of a FAT/FAT32
SD card, named `PROG1.TXT` … `PROG4.TXT`. Button **P1** runs `PROG1.TXT`,
button **P2** runs `PROG2.TXT`, and so on. Files are read again each time a
button is pressed, so you can edit them on a PC and put the card back
without restarting the panel.

## Commands

One command per line. Commands are case-insensitive and may be indented.

| Command            | Meaning                                                      |
|--------------------|--------------------------------------------------------------|
| `NAME <text>`      | Name shown on the LCD (first 16 characters). Optional.       |
| `ON <seconds>`     | Power the siren motor for the given time.                    |
| `OFF <seconds>`    | Leave the siren unpowered (it coasts down) for the given time. |
| `WAIT <seconds>`   | Same as `OFF`.                                               |
| `REPEAT <n>`       | Repeat the block up to the matching `END` *n* times (1–9999). |
| `REPEAT FOREVER`   | Repeat the block until STOP is pressed.                      |
| `END`              | Ends a `REPEAT` block.                                       |

* **Durations** are in seconds, with up to three decimals: `5`, `2.5`,
  `0.25`. You can add an `s` suffix if you like (`5s`). The smallest step
  is 1 ms and the largest is 24 hours.
* **`REPEAT` blocks can be nested** up to 4 levels deep. Each block must
  contain at least one `ON`/`OFF`/`WAIT`.
* **Comments** start with `#` or `;` and run to the end of the line.
* Up to **64 commands** per file (each `REPEAT` and `END` counts as one),
  and a file can be at most **2 KB**.

## Example

```
# Air raid warning: rising and falling tone, 3 minutes
NAME Air Raid
REPEAT 15
  ON 8     ; spin up
  OFF 4    ; coast down
END
```

## How sound relates to ON/OFF

A motor siren takes time to spin up and down. During `ON` the pitch rises
towards full speed; during `OFF` it falls. Short cycles (e.g. `ON 3` /
`OFF 2`) give a wailing tone that never reaches full pitch; long `ON`
steps give a steady tone. Tune the timings on your own siren.

## Errors

If a file can't be used, the LCD shows the program number and the reason
for 3 seconds, and the siren stays off:

| LCD                         | Meaning                                    |
|-----------------------------|--------------------------------------------|
| `P2 ERROR` / `No PROG2.TXT` | File not found in the card's root folder   |
| `P2 LINE 7` / `Bad command` | Unknown command on line 7                  |
| `P2 LINE 7` / `Bad value`   | Missing, zero or out-of-range number       |
| `P2 LINE 7` / `REPEAT/END`  | `END` without `REPEAT`, or `REPEAT` never closed |
| `P2 LINE 7` / `Nested too deep` | More than 4 nested `REPEAT`s           |
| `P2 LINE 7` / `Too many steps`  | More than 64 commands                  |
| `P2 ERROR` / `No ON/OFF steps`  | Nothing to play                        |
| `P2 ERROR` / `Too long`     | Total duration exceeds ~49 days            |
| `P2 ERROR` / `File too big` | File larger than 2 KB                      |
| `NO SD CARD`                | Card missing or not responding             |
| `SD CARD ERROR`             | Card is not formatted FAT/FAT32 (exFAT is not supported) |
