/*
 * Application-level configuration (behaviour, not pin mapping).
 * Pin mapping lives in firmware/core/board.h.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Number of program buttons; button N loads PROG<N>.TXT from the SD card. */
#define APP_NUM_PROGRAMS        4

/* Main loop period. Buttons are sampled at this rate. */
#define APP_TICK_MS             10

/* Thermal protection for manual mode: if the MANUAL button is held longer
 * than this, the siren is switched off until the button is released.
 * Set to 0 to disable. */
#define APP_MANUAL_MAX_ON_MS    (120UL * 1000UL)

/* How long "DONE" / error messages stay on the LCD. */
#define APP_MESSAGE_MS          3000

/* Pressing the button of the program that is already running stops it. */
#define APP_SAME_BUTTON_STOPS   1

#endif /* APP_CONFIG_H */
