#ifndef PORT_STM32_H
#define PORT_STM32_H

/* Initialise SPI (SD card), I2C (LCD) and the drivers using them. */
void port_init(void);

void Error_Handler(void);

#endif /* PORT_STM32_H */
