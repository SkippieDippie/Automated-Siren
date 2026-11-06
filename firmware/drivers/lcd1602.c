#include "lcd1602.h"

/* PCF8574 -> HD44780 wiring used by virtually all backpacks:
 * P0=RS P1=RW P2=E P3=Backlight P4..P7=D4..D7 */
#define PIN_RS  0x01U
#define PIN_E   0x04U
#define PIN_BL  0x08U

#define I2C_TIMEOUT_MS 20

static I2C_HandleTypeDef *s_i2c;
static uint16_t s_addr;          /* 8-bit HAL address */
static uint8_t  s_backlight = PIN_BL;
static bool     s_ok;

/* Encode one byte as two 4-bit transfers, each with an E pulse:
 * 4 expander writes per byte. */
static uint8_t *encode(uint8_t *p, uint8_t value, uint8_t flags)
{
    uint8_t hi = (uint8_t)((value & 0xF0U) | flags | s_backlight);
    uint8_t lo = (uint8_t)(((value << 4) & 0xF0U) | flags | s_backlight);
    *p++ = (uint8_t)(hi | PIN_E);
    *p++ = hi;
    *p++ = (uint8_t)(lo | PIN_E);
    *p++ = lo;
    return p;
}

static void send(const uint8_t *buf, uint16_t len)
{
    if (s_ok) {
        (void)HAL_I2C_Master_Transmit(s_i2c, s_addr, (uint8_t *)buf, len, I2C_TIMEOUT_MS);
    }
}

static void command(uint8_t cmd)
{
    uint8_t buf[4];
    send(buf, (uint16_t)(encode(buf, cmd, 0) - buf));
}

/* Init-sequence nibble (8-bit mode commands before switching to 4-bit). */
static void nibble(uint8_t hi)
{
    uint8_t buf[2] = { (uint8_t)(hi | PIN_E | s_backlight), (uint8_t)(hi | s_backlight) };
    send(buf, 2);
}

bool lcd_init(I2C_HandleTypeDef *i2c, const uint8_t *addrs, uint8_t n_addrs)
{
    s_i2c = i2c;
    s_ok = false;
    for (uint8_t i = 0; i < n_addrs; i++) {
        uint16_t a = (uint16_t)(addrs[i] << 1);
        if (HAL_I2C_IsDeviceReady(i2c, a, 2, I2C_TIMEOUT_MS) == HAL_OK) {
            s_addr = a;
            s_ok = true;
            break;
        }
    }
    if (!s_ok) {
        return false;
    }

    HAL_Delay(50);                    /* power-on wait          */
    nibble(0x30); HAL_Delay(5);       /* HD44780 reset sequence */
    nibble(0x30); HAL_Delay(1);
    nibble(0x30); HAL_Delay(1);
    nibble(0x20); HAL_Delay(1);       /* 4-bit mode             */

    command(0x28);                    /* 4-bit, 2 lines, 5x8    */
    command(0x0C);                    /* display on, no cursor  */
    command(0x06);                    /* entry: increment       */
    lcd_clear();
    return true;
}

void lcd_clear(void)
{
    command(0x01);
    HAL_Delay(2);
}

void lcd_backlight(bool on)
{
    s_backlight = on ? PIN_BL : 0U;
    uint8_t b = s_backlight;
    send(&b, 1);
}

void lcd_write_line(uint8_t row, const char *text)
{
    /* Address command + 16 chars sent as one I2C transaction (~7 ms at
     * 100 kHz), so the main loop is not held up character by character.
     * An I2C byte (~90 us) is longer than any HD44780 data write needs. */
    uint8_t buf[4 * 17];
    uint8_t *p = encode(buf, (uint8_t)(0x80U | (row ? 0x40U : 0x00U)), 0);
    bool ended = false;
    for (uint8_t i = 0; i < 16; i++) {
        ended = ended || text[i] == '\0';
        p = encode(p, (uint8_t)(ended ? ' ' : text[i]), PIN_RS);
    }
    send(buf, (uint16_t)(p - buf));
}
