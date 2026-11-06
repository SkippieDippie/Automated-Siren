#ifndef PORT_STM32_H
#define PORT_STM32_H

#include <stdint.h>

#include "system.h"

/* Initialise button shift register, LCD and SD card interface. */
void port_init(void);

/* Raw 74HC165 input levels (bit set = input high = button released),
 * without the active-low decoding or fault check. For diagnostics. */
uint32_t port_buttons_levels(void);

#endif /* PORT_STM32_H */
