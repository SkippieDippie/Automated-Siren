/*
 * STM32 implementation of the application port (app_port.h).
 */
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_port.h"
#include "buttons.h"
#include "board.h"
#include "ff.h"
#include "hc165.h"
#include "lcd1602.h"
#include "port_stm32.h"
#include "sd_spi.h"

/* Largest program file accepted (bytes). */
#define PROGRAM_TEXT_MAX 2048

static SPI_HandleTypeDef hspi;
static FATFS fs;
static char text_buf[PROGRAM_TEXT_MAX + 1];

static const hc165_t s_buttons = {
    .load_port = BTN_LOAD_PORT, .load_pin = BTN_LOAD_PIN,
    .clk_port  = BTN_CLK_PORT,  .clk_pin  = BTN_CLK_PIN,
    .data_port = BTN_DATA_PORT, .data_pin = BTN_DATA_PIN,
    .chips     = APP_BUTTON_CHIPS,
};

static const lcd_pins_t s_lcd_pins = LCD_PINS;

/* ------------------------------------------------------------------ */
/* Peripheral init                                                     */
/* ------------------------------------------------------------------ */

static void spi_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_SPI1_CLK_ENABLE();

    /* CS high (deselected) before it becomes an output. */
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    g.Pin = SD_CS_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SD_CS_PORT, &g);

    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;          /* SCK, MOSI */
    g.Mode = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_6;                       /* MISO */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;                     /* reads 0xFF with no card */
    HAL_GPIO_Init(GPIOA, &g);

    hspi.Instance = SD_SPI;
    hspi.Init.Mode = SPI_MODE_MASTER;
    hspi.Init.Direction = SPI_DIRECTION_2LINES;
    hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi.Init.NSS = SPI_NSS_SOFT;
    hspi.Init.BaudRatePrescaler = SD_SPI_SLOW_PRESC;
    hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    if (HAL_SPI_Init(&hspi) != HAL_OK) {
        Error_Handler();
    }
    __HAL_SPI_ENABLE(&hspi);
    sd_spi_attach(&hspi);
}

void port_init(void)
{
    hc165_init(&s_buttons);
    lcd_init(&s_lcd_pins);
    spi_init();
}

/* ------------------------------------------------------------------ */
/* app_port.h                                                          */
/* ------------------------------------------------------------------ */

uint32_t port_millis(void)
{
    return HAL_GetTick();
}

uint32_t port_buttons_levels(void)
{
    return hc165_read(&s_buttons);
}

uint32_t port_buttons_raw(void)
{
    bool fault;
    uint32_t levels = hc165_read(&s_buttons);
    /* A stuck-low data line reads as "nothing pressed" (see buttons.h). */
    return buttons_decode_active_low(levels, 8U * APP_BUTTON_CHIPS, &fault);
}

void port_siren_set(bool on)
{
    bool level = SIREN_ACTIVE_HIGH ? on : !on;
    HAL_GPIO_WritePin(SIREN_PORT, SIREN_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void port_lcd_line(uint8_t row, const char *text)
{
    lcd_write_line(row, text);
}

bool port_sd_mount(void)
{
    /* Force a fresh mount so a swapped card is picked up. */
    return f_mount(&fs, "", 1) == FR_OK;
}

load_status_t port_load_program(uint8_t slot, program_t *out, program_result_t *res)
{
    FIL f;
    UINT n = 0;
    char path[16];

    FRESULT fr = f_mount(&fs, "", 1);
    if (fr == FR_NOT_READY || fr == FR_DISK_ERR) {
        return LOAD_NO_CARD;
    }
    if (fr != FR_OK) {
        return LOAD_NO_FS;
    }

    snprintf(path, sizeof path, "PROG%u.TXT", (unsigned)(slot + 1));
    fr = f_open(&f, path, FA_READ);
    if (fr == FR_NO_FILE || fr == FR_NO_PATH) {
        return LOAD_NO_FILE;
    }
    if (fr != FR_OK) {
        return LOAD_READ_ERR;
    }

    if (f_size(&f) > PROGRAM_TEXT_MAX) {
        (void)f_close(&f);
        return LOAD_TOO_BIG;
    }
    fr = f_read(&f, text_buf, PROGRAM_TEXT_MAX, &n);
    (void)f_close(&f);
    if (fr != FR_OK) {
        return LOAD_READ_ERR;
    }
    text_buf[n] = '\0';

    *res = program_parse(text_buf, out);
    return res->code == PROG_OK ? LOAD_OK : LOAD_PARSE_ERR;
}
