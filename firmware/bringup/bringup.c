/*
 * Hardware bring-up firmware for the siren control panel.
 *
 * Checks each part of the board in turn, reporting on the LCD and on the
 * serial log (USART1 TX = PA9, 115200 8N1). See docs/bringup.md.
 *
 *   Page 1 BUTTONS  live state of every 74HC165 input
 *   Page 2 SIREN    hold MANUAL to energise the siren output (max 10 s)
 *   Page 3 SD CARD  card type/size and a parse check of every PROGn.TXT
 *   Page 4 INFO     clock source, uptime, reset cause
 *
 * Hold STOP for 1 s to go to the next page. The siren output can only
 * be energised on the SIREN page.
 */
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_port.h"
#include "board.h"
#include "buttons.h"
#include "ff.h"
#include "port_stm32.h"
#include "program.h"
#include "sd_spi.h"
#include "system.h"
#include "uart_log.h"

#define TICK_MS           10U
#define PAGE_HOLD_MS      1000U
#define SIREN_MAX_ON_MS   10000U
#define N_INPUTS          (8U * APP_BUTTON_CHIPS)

typedef enum { PAGE_BUTTONS = 0, PAGE_SIREN, PAGE_SD, PAGE_INFO, PAGE_COUNT } page_t;

static const char *const s_page_title[PAGE_COUNT] = {
    "BUTTONS", "SIREN OUT", "SD CARD", "INFO"
};

static clock_source_t s_clock;
static const char    *s_reset_cause = "?";
static char           s_sd_line[40] = "not tested";   /* LCD shows 16 */
static char           s_shown[2][17];

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void lcd(uint8_t row, const char *fmt_text)
{
    char line[17];
    size_t n = strlen(fmt_text);
    if (n > 16) {
        n = 16;
    }
    memcpy(line, fmt_text, n);
    memset(line + n, ' ', 16 - n);
    line[16] = '\0';
    if (memcmp(line, s_shown[row], 17) != 0) {
        memcpy(s_shown[row], line, 17);
        port_lcd_line(row, line);
    }
}

static void lcd_force(uint8_t row, const char *text)
{
    s_shown[row][0] = '\0';
    lcd(row, text);
}

static void button_name(unsigned idx, char *out, size_t len)
{
    if (idx == BTN_MANUAL) {
        snprintf(out, len, "MANUAL");
    } else if (idx == BTN_STOP) {
        snprintf(out, len, "STOP");
    } else {
        snprintf(out, len, "P%u", idx - BTN_PROG1 + 1U);
    }
}

static const char *read_reset_cause(void)
{
    const char *cause = "?";
    uint32_t csr = RCC->CSR;
    if (csr & RCC_CSR_IWDGRSTF)      cause = "WDOG";   /* main firmware hung */
    else if (csr & RCC_CSR_WWDGRSTF) cause = "WWDG";
    else if (csr & RCC_CSR_SFTRSTF)  cause = "SOFT";
    else if (csr & RCC_CSR_LPWRRSTF) cause = "LPWR";
    else if (csr & RCC_CSR_PORRSTF)  cause = "POWER";
    else if (csr & RCC_CSR_PINRSTF)  cause = "PIN";
    RCC->CSR |= RCC_CSR_RMVF;        /* clear for next time */
    return cause;
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/* Visual LCD test: user checks every cell lights and text is readable. */
static void lcd_test(void)
{
    uart_log("[LCD] Test pattern: all blocks, then characters, then title.");
    uart_log("[LCD] If you see nothing, adjust the contrast pot (V0).");
    uart_log("[LCD] Only black blocks on row 1 = LCD not initialised:");
    uart_log("[LCD]   check RS/E/D4-D7 wiring and that RW is tied to GND.");

    const char blocks[17] = "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF";
    lcd_force(0, blocks);
    lcd_force(1, blocks);
    HAL_Delay(1500);
    lcd_force(0, "0123456789ABCDEF");
    lcd_force(1, "abcdefghijklmnop");
    HAL_Delay(1500);
    lcd_force(0, "SIREN BRING-UP");
    lcd_force(1, "Hold STOP: page");
    HAL_Delay(2000);
}

static void sd_test(void)
{
    static FATFS fs;
    static program_t prog;

    uart_log("[SD] Testing card...");
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        if (fr == FR_NOT_READY || fr == FR_DISK_ERR) {
            uart_log("[SD] FAIL: no card or card not responding (FatFs %d).", (int)fr);
            uart_log("[SD]   check CS=PA4 SCK=PA5 MISO=PA6 MOSI=PA7, 3.3V power.");
            snprintf(s_sd_line, sizeof s_sd_line, "NO CARD");
        } else if (fr == FR_NO_FILESYSTEM) {
            uart_log("[SD] FAIL: card responds but has no FAT16/FAT32 filesystem.");
            snprintf(s_sd_line, sizeof s_sd_line, "NOT FAT16/32");
        } else {
            uart_log("[SD] FAIL: mount error (FatFs %d).", (int)fr);
            snprintf(s_sd_line, sizeof s_sd_line, "MOUNT ERR %d", (int)fr);
        }
        return;
    }

    const char *type = fs.fs_type == FS_FAT32 ? "FAT32" :
                       fs.fs_type == FS_FAT16 ? "FAT16" :
                       fs.fs_type == FS_FAT12 ? "FAT12" : "FAT?";
    uint32_t mb = sd_spi_sector_count() / 2048U;
    uart_log("[SD] OK: %s, %lu MB.", type, (unsigned long)mb);

    unsigned ok = 0;
    for (uint8_t slot = 0; slot < APP_NUM_PROGRAMS; slot++) {
        program_result_t res = { PROG_OK, 0 };
        load_status_t st = port_load_program(slot, &prog, &res);
        unsigned n = slot + 1U;
        switch (st) {
        case LOAD_OK: {
            ok++;
            if (prog.total_ms == PROGRAM_DURATION_INFINITE) {
                uart_log("[SD]   PROG%u.TXT  OK  \"%s\"  %u steps, runs until STOP",
                         n, prog.name, prog.count);
            } else {
                unsigned long s = (prog.total_ms + 999UL) / 1000UL;
                uart_log("[SD]   PROG%u.TXT  OK  \"%s\"  %u steps, %lu:%02lu",
                         n, prog.name, prog.count, s / 60UL, s % 60UL);
            }
            break;
        }
        case LOAD_NO_FILE:
            uart_log("[SD]   PROG%u.TXT  missing", n);
            break;
        case LOAD_TOO_BIG:
            uart_log("[SD]   PROG%u.TXT  too big (max 2 KB)", n);
            break;
        case LOAD_PARSE_ERR:
            uart_log("[SD]   PROG%u.TXT  ERROR line %u: %s", n, res.line,
                     program_err_str(res.code));
            break;
        default:
            uart_log("[SD]   PROG%u.TXT  read error (%d)", n, (int)st);
            break;
        }
    }
    uart_log("[SD] %u of %u programs OK.", ok, (unsigned)APP_NUM_PROGRAMS);

    char size[8];
    if (mb >= 10240U) {
        snprintf(size, sizeof size, "%luG", (unsigned long)(mb / 1024U));
    } else {
        snprintf(size, sizeof size, "%lu.%luG", (unsigned long)(mb / 1024U),
                 (unsigned long)((mb % 1024U) * 10U / 1024U));
    }
    snprintf(s_sd_line, sizeof s_sd_line, "%s %s %u/%u", type, size, ok,
             (unsigned)APP_NUM_PROGRAMS);
}

/* ------------------------------------------------------------------ */
/* Main loop                                                           */
/* ------------------------------------------------------------------ */

int main(void)
{
    HAL_Init();
    system_gpio_init();                 /* siren output forced off first */
    s_clock = system_clock_init();
    s_reset_cause = read_reset_cause();

    uart_log_init(115200);
    uart_log("%s", "");
    uart_log("=== Siren panel bring-up ===");
    uart_log("[MCU] Clock: %s, SYSCLK %lu Hz", s_clock == CLOCK_HSE_72MHZ ?
             "8 MHz crystal (HSE) x9" : "INTERNAL HSI - crystal did not start!",
             (unsigned long)SystemCoreClock);
    uart_log("[MCU] Reset cause: %s", s_reset_cause);
    uart_log("[MCU] Button chips configured: %u (%u inputs)",
             (unsigned)APP_BUTTON_CHIPS, (unsigned)N_INPUTS);

    port_init();
    lcd_test();

    lcd_force(0, "SD CARD TEST...");
    lcd_force(1, "");
    sd_test();

    uart_log("[BTN] Press each button; changes are logged here.");
    uart_log("[UI] Hold STOP 1 s for next page. Siren output only on SIREN page.");

    buttons_t btn;
    buttons_init(&btn);
    page_t page = PAGE_BUTTONS;
    uint32_t stop_since = 0;
    bool page_switched = false;
    bool siren = false;
    bool siren_timeout = false;
    uint32_t siren_since = 0;
    uint32_t last_levels = 0xFFFFFFFFUL;
    bool last_fault = false;
    char l1[24];
    char l2[24];

    uint32_t next = HAL_GetTick();
    for (;;) {
        uint32_t now = HAL_GetTick();

        /* --- Inputs --- */
        bool fault;
        uint32_t levels = port_buttons_levels();
        uint32_t pressed = buttons_decode_active_low(levels, N_INPUTS, &fault);
        buttons_update(&btn, pressed);

        if (levels != last_levels || fault != last_fault) {
            char list[100] = "";
            size_t len = 0;
            for (unsigned i = 0; i < N_INPUTS; i++) {
                if (pressed & (1UL << i)) {
                    char name[8];
                    button_name(i, name, sizeof name);
                    len += (size_t)snprintf(list + len, sizeof list - len, " %s", name);
                    if (len >= sizeof list) {
                        break;
                    }
                }
            }
            uart_log("[BTN] levels=0x%0*lX pressed:%s%s", (int)(APP_BUTTON_CHIPS * 2),
                     (unsigned long)levels, list[0] ? list : " none",
                     fault ? "  ** FAULT: all inputs low - data line stuck low? **" : "");
            last_levels = levels;
            last_fault = fault;
        }

        /* --- Page navigation: hold STOP --- */
        if (buttons_is_down(&btn, BTN_STOP)) {
            if (buttons_was_pressed(&btn, BTN_STOP)) {
                stop_since = now;
                page_switched = false;
            } else if (!page_switched && now - stop_since >= PAGE_HOLD_MS) {
                page = (page_t)((page + 1) % PAGE_COUNT);
                page_switched = true;
                uart_log("[UI] Page %u: %s", (unsigned)page + 1U, s_page_title[page]);
                if (page == PAGE_SD) {
                    lcd(1, "testing...");
                    sd_test();
                }
            }
        }

        /* --- Siren output: only on the SIREN page, with a time limit --- */
        bool want = page == PAGE_SIREN && buttons_is_down(&btn, BTN_MANUAL);
        if (!want) {
            siren_timeout = false;
        } else if (!siren && !siren_timeout) {
            siren_since = now;
        }
        if (want && !siren_timeout && siren && now - siren_since >= SIREN_MAX_ON_MS) {
            siren_timeout = true;
            uart_log("[SIREN] 10 s limit reached, output OFF. Release MANUAL.");
        }
        bool on = want && !siren_timeout;
        if (on != siren) {
            siren = on;
            port_siren_set(on);
            if (!siren_timeout) {
                uart_log("[SIREN] Output %s (PB1 %s)", on ? "ON" : "OFF",
                         (on == (SIREN_ACTIVE_HIGH != 0)) ? "high" : "low");
            }
        }

        /* --- SD retest --- */
        if (page == PAGE_SD && buttons_was_pressed(&btn, BTN_PROG1)) {
            lcd(1, "testing...");
            sd_test();
        }

        /* --- Heartbeat LED (except on SIREN page, where it mirrors the output) --- */
        if (page != PAGE_SIREN) {
            HAL_GPIO_WritePin(LED_PORT, LED_PIN, ((now / 500U) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        /* --- Display --- */
        snprintf(l1, sizeof l1, "%-12s %u/%u", s_page_title[page], (unsigned)page + 1U,
                 (unsigned)PAGE_COUNT);
        switch (page) {
        case PAGE_BUTTONS: {
            /* Chip 0 inputs A..H as M S 1 2 3 4 5 6; '.' = released. */
            static const char labels[] = "MS123456";
            for (unsigned i = 0; i < 8; i++) {
                l2[i] = (pressed & (1UL << i)) ? labels[i] : '.';
            }
            uint32_t others = pressed >> 8;
            unsigned n_others = 0;
            while (others) {
                n_others += others & 1U;
                others >>= 1;
            }
            if (fault) {
                snprintf(l2 + 8, sizeof l2 - 8, " FAULT!");
            } else if (n_others) {
                snprintf(l2 + 8, sizeof l2 - 8, " +%u", n_others);
            } else {
                l2[8] = '\0';
            }
            break;
        }
        case PAGE_SIREN:
            if (siren_timeout) {
                snprintf(l2, sizeof l2, "LIMIT: release");
            } else {
                snprintf(l2, sizeof l2, "MANUAL: %s", siren ? "ON" : "off");
            }
            break;
        case PAGE_SD:
            snprintf(l2, sizeof l2, "%.16s", s_sd_line);
            break;
        case PAGE_INFO:
        default: {
            unsigned long s = now / 1000UL;
            snprintf(l1, sizeof l1, "%-12s %u/%u",
                     s_clock == CLOCK_HSE_72MHZ ? "72MHz HSE" : "64MHz HSI!",
                     (unsigned)page + 1U, (unsigned)PAGE_COUNT);
            snprintf(l2, sizeof l2, "%lu:%02lu:%02lu %s", s / 3600UL, (s / 60UL) % 60UL,
                     s % 60UL, s_reset_cause);
            break;
        }
        }
        lcd(0, l1);
        lcd(1, l2);

        /* --- Fixed tick --- */
        next += TICK_MS;
        if ((int32_t)(HAL_GetTick() - next) > (int32_t)TICK_MS) {
            next = HAL_GetTick();
        }
        while ((int32_t)(HAL_GetTick() - next) < 0) {
            __WFI();
        }
    }
}
