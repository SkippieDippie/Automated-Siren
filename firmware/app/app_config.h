/*
 * Application-level configuration (behaviour, not pin mapping).
 * Pin mapping lives in firmware/core/board.h.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Number of 74HC165 shift registers chained for button input (1-4).
 * Each chip gives 8 inputs: the first two are MANUAL and STOP, the rest
 * are program buttons. */
#ifndef APP_BUTTON_CHIPS
#define APP_BUTTON_CHIPS        1
#endif

/* Number of program buttons; button N loads PROG<N>.TXT from the SD card.
 * Defaults to every remaining input on the shift register chain. */
#ifndef APP_NUM_PROGRAMS
#define APP_NUM_PROGRAMS        (APP_BUTTON_CHIPS * 8 - 2)
#endif

#if APP_BUTTON_CHIPS < 1 || APP_BUTTON_CHIPS > 4
#error "APP_BUTTON_CHIPS must be 1..4"
#endif
#if APP_NUM_PROGRAMS < 1 || APP_NUM_PROGRAMS > APP_BUTTON_CHIPS * 8 - 2
#error "APP_NUM_PROGRAMS does not fit on the shift register chain"
#endif

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
