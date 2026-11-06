#include "sd_spi.h"

#include "board.h"

#define CMD0   0U    /* GO_IDLE_STATE        */
#define CMD8   8U    /* SEND_IF_COND         */
#define CMD9   9U    /* SEND_CSD             */
#define CMD12  12U   /* STOP_TRANSMISSION    */
#define CMD16  16U   /* SET_BLOCKLEN         */
#define CMD17  17U   /* READ_SINGLE_BLOCK    */
#define CMD55  55U   /* APP_CMD              */
#define CMD58  58U   /* READ_OCR             */
#define ACMD41 41U   /* SD_SEND_OP_COND      */

#define TOKEN_START_BLOCK 0xFEU

static SPI_HandleTypeDef *s_spi;
static bool s_ready;
static bool s_block_addr;   /* SDHC/SDXC: LBA addressing, else byte address */

/* ------------------------------------------------------------------ */

static void cs_low(void)  { HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET); }
static void cs_high(void) { HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET); }

static uint8_t xfer(uint8_t out)
{
    uint8_t in = 0xFF;
    (void)HAL_SPI_TransmitReceive(s_spi, &out, &in, 1, 10);
    return in;
}

static void set_speed(uint32_t prescaler)
{
    __HAL_SPI_DISABLE(s_spi);
    s_spi->Init.BaudRatePrescaler = prescaler;
    MODIFY_REG(s_spi->Instance->CR1, SPI_CR1_BR, prescaler);
    __HAL_SPI_ENABLE(s_spi);
}

static bool wait_ready(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    do {
        if (xfer(0xFF) == 0xFF) {
            return true;
        }
    } while (HAL_GetTick() - t0 < timeout_ms);
    return false;
}

static void deselect(void)
{
    cs_high();
    (void)xfer(0xFF);   /* extra clocks so the card releases MISO */
}

static bool select(void)
{
    cs_low();
    (void)xfer(0xFF);
    if (wait_ready(500)) {
        return true;
    }
    deselect();
    return false;
}

/* Send a command, return the R1 response (0xFF on timeout). Leaves CS
 * asserted so the caller can read trailing data. */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    if (cmd & 0x80U) {   /* ACMD<n> = CMD55 + CMD<n> */
        uint8_t r = send_cmd(CMD55, 0);
        if (r > 1U) {
            return r;
        }
        cmd &= 0x7FU;
    }

    deselect();
    if (!select()) {
        return 0xFF;
    }

    uint8_t crc = 0x01U;                 /* dummy CRC + stop bit */
    if (cmd == CMD0) crc = 0x95U;        /* CRC only checked for CMD0/CMD8 */
    if (cmd == CMD8) crc = 0x87U;

    (void)xfer((uint8_t)(0x40U | cmd));
    (void)xfer((uint8_t)(arg >> 24));
    (void)xfer((uint8_t)(arg >> 16));
    (void)xfer((uint8_t)(arg >> 8));
    (void)xfer((uint8_t)arg);
    (void)xfer(crc);

    if (cmd == CMD12) {
        (void)xfer(0xFF);                /* skip stuff byte */
    }

    uint8_t r;
    uint8_t n = 10;
    do {
        r = xfer(0xFF);
    } while ((r & 0x80U) && --n);
    return r;
}

static bool read_data(uint8_t *buf, uint16_t len)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t token;
    do {
        token = xfer(0xFF);
    } while (token == 0xFF && HAL_GetTick() - t0 < 200U);
    if (token != TOKEN_START_BLOCK) {
        return false;
    }

    /* Bulk receive: MOSI must stay high, so transmit from an 0xFF buffer
     * in chunks. */
    static const uint8_t ff[64] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    };
    while (len > 0) {
        uint16_t n = len > sizeof ff ? (uint16_t)sizeof ff : len;
        if (HAL_SPI_TransmitReceive(s_spi, (uint8_t *)ff, buf, n, 50) != HAL_OK) {
            return false;
        }
        buf += n;
        len = (uint16_t)(len - n);
    }
    (void)xfer(0xFF);   /* discard CRC */
    (void)xfer(0xFF);
    return true;
}

/* ------------------------------------------------------------------ */

void sd_spi_attach(SPI_HandleTypeDef *spi)
{
    s_spi = spi;
}

bool sd_spi_init(void)
{
    uint8_t ocr[4];

    s_ready = false;
    set_speed(SD_SPI_SLOW_PRESC);

    /* >= 74 clocks with CS high to enter native mode. */
    cs_high();
    for (int i = 0; i < 10; i++) {
        (void)xfer(0xFF);
    }

    if (send_cmd(CMD0, 0) != 0x01U) {
        deselect();
        return false;
    }

    uint32_t t0 = HAL_GetTick();
    if (send_cmd(CMD8, 0x1AAU) == 0x01U) {
        /* SD v2: check voltage range echo. */
        for (int i = 0; i < 4; i++) {
            ocr[i] = xfer(0xFF);
        }
        if (ocr[2] != 0x01U || ocr[3] != 0xAAU) {
            deselect();
            return false;
        }
        /* Initialise with HCS bit set. */
        while (send_cmd(0x80U | ACMD41, 1UL << 30) != 0U) {
            if (HAL_GetTick() - t0 > 1000U) {
                deselect();
                return false;
            }
        }
        if (send_cmd(CMD58, 0) != 0U) {
            deselect();
            return false;
        }
        for (int i = 0; i < 4; i++) {
            ocr[i] = xfer(0xFF);
        }
        s_block_addr = (ocr[0] & 0x40U) != 0U;   /* CCS bit */
    } else {
        /* SD v1 (old, <= 2 GB). MMC is not supported. */
        while (send_cmd(0x80U | ACMD41, 0) != 0U) {
            if (HAL_GetTick() - t0 > 1000U) {
                deselect();
                return false;
            }
        }
        s_block_addr = false;
    }

    if (!s_block_addr && send_cmd(CMD16, 512) != 0U) {
        deselect();
        return false;
    }

    deselect();
    set_speed(SD_SPI_FAST_PRESC);
    s_ready = true;
    return true;
}

bool sd_spi_is_ready(void)
{
    return s_ready;
}

bool sd_spi_read(uint8_t *buf, uint32_t lba, uint32_t count)
{
    if (!s_ready) {
        return false;
    }
    while (count--) {
        uint32_t addr = s_block_addr ? lba : lba * 512U;
        if (send_cmd(CMD17, addr) != 0U || !read_data(buf, 512)) {
            deselect();
            return false;
        }
        buf += 512;
        lba++;
    }
    deselect();
    return true;
}

uint32_t sd_spi_sector_count(void)
{
    uint8_t csd[16];
    uint32_t sectors = 0;

    if (!s_ready || send_cmd(CMD9, 0) != 0U || !read_data(csd, 16)) {
        deselect();
        return 0;
    }
    deselect();

    if ((csd[0] >> 6) == 1U) {   /* CSD v2 (SDHC/SDXC) */
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3FU) << 16) |
                          ((uint32_t)csd[8] << 8) | csd[9];
        sectors = (c_size + 1U) << 10;
    } else {                     /* CSD v1 */
        uint32_t c_size = ((uint32_t)(csd[6] & 0x03U) << 10) |
                          ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
        uint8_t mult = (uint8_t)(((csd[9] & 0x03U) << 1) | (csd[10] >> 7));
        uint8_t read_bl_len = csd[5] & 0x0FU;
        sectors = (c_size + 1U) << (mult + 2U + read_bl_len - 9U);
    }
    return sectors;
}
