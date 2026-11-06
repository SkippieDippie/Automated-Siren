/*
 * SD / SDHC / SDXC card driver over SPI (read-only, single-block reads).
 */
#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

void sd_spi_attach(SPI_HandleTypeDef *spi);

/* Initialise the card. Returns true on success. Safe to call repeatedly
 * (e.g. after the card was swapped). */
bool sd_spi_init(void);

bool sd_spi_is_ready(void);

/* Read `count` 512-byte sectors starting at `lba`. */
bool sd_spi_read(uint8_t *buf, uint32_t lba, uint32_t count);

/* Number of 512-byte sectors (0 if unknown). */
uint32_t sd_spi_sector_count(void);

#endif /* SD_SPI_H */
