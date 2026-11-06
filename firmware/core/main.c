/*
 * Siren control panel - STM32F103 entry point.
 */
#include "board.h"
#include "app.h"
#include "port_stm32.h"
#include "system.h"

static IWDG_HandleTypeDef hiwdg;

static void watchdog_init(void);

int main(void)
{
    HAL_Init();

    /* Outputs first: the siren must be off before anything else runs. */
    system_gpio_init();
    (void)system_clock_init();

    port_init();          /* buttons, LCD, SD */
    watchdog_init();

    static app_t app;
    app_init(&app);

    uint32_t next = HAL_GetTick();
    for (;;) {
        app_tick(&app);
        HAL_IWDG_Refresh(&hiwdg);

        next += APP_TICK_MS;
        /* If we fell behind (e.g. slow SD read), resynchronise. */
        if ((int32_t)(HAL_GetTick() - next) > (int32_t)APP_TICK_MS) {
            next = HAL_GetTick();
        }
        while ((int32_t)(HAL_GetTick() - next) < 0) {
            __WFI();      /* sleep until next SysTick */
        }
    }
}

static void watchdog_init(void)
{
    /* LSI ~40 kHz / 256 = ~156 Hz -> counts per ms ~0.156 */
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = (WATCHDOG_TIMEOUT_MS * 40U) / 256U;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
#ifdef DEBUG
    __HAL_DBGMCU_FREEZE_IWDG();   /* don't reset while halted in debugger */
#endif
}
