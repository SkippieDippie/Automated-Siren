/*
 * STM32 implementation of the application port (app_port.h).
 */
#include <stdio.h>
#include <string.h>

#include "app_port.h"
#include "board.h"
#include "ff.h"
#include "lcd1602.h"
#include "port_stm32.h"
#include "sd_spi.h"

/* Largest program file accepted (bytes). */
#define PROGRAM_TEXT_MAX 2048

static SPI_HandleTypeDef hspi;
static I2C_HandleTypeDef hi2c;
static FATFS fs;
static char text_buf[PROGRAM_TEXT_MAX + 1];

static const board_pin_t s_buttons[] = BOARD_BUTTONS;

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

static void i2c_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_I2C1_CLK_ENABLE();
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;          /* SCL, SDA */
    g.Mode = GPIO_MODE_AF_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);

    hi2c.Instance = LCD_I2C;
    hi2c.Init.ClockSpeed = LCD_I2C_SPEED_HZ;
    hi2c.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c.Init.OwnAddress1 = 0;
    hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c) != HAL_OK) {
        Error_Handler();
    }
}

void port_init(void)
{
    static const uint8_t lcd_addrs[] = LCD_I2C_ADDRS;

    spi_init();
    i2c_init();
    (void)lcd_init(&hi2c, lcd_addrs, (uint8_t)sizeof lcd_addrs);
}

/* ------------------------------------------------------------------ */
/* app_port.h                                                          */
/* ------------------------------------------------------------------ */

uint32_t port_millis(void)
{
    return HAL_GetTick();
}

uint16_t port_buttons_raw(void)
{
    uint16_t mask = 0;
    for (unsigned i = 0; i < sizeof s_buttons / sizeof s_buttons[0]; i++) {
        if (HAL_GPIO_ReadPin(s_buttons[i].port, s_buttons[i].pin) == GPIO_PIN_RESET) {
            mask |= (uint16_t)(1U << i);
        }
    }
    return mask;
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
