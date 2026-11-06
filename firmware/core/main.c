/*
 * Siren control panel - STM32F103 entry point and peripheral setup.
 */
#include "board.h"
#include "app.h"
#include "port_stm32.h"

static IWDG_HandleTypeDef hiwdg;

static void clock_init(void);
static void gpio_init(void);
static void watchdog_init(void);

int main(void)
{
    HAL_Init();

    /* Outputs first: the siren must be off before anything else runs. */
    gpio_init();
    clock_init();

    port_init();          /* SPI, I2C, LCD, SD */
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

/* 8 MHz HSE x9 -> 72 MHz SYSCLK, APB1 36 MHz, APB2 72 MHz. */
static void clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        /* No crystal? Fall back to HSI/2 x16 = 64 MHz so the panel still works. */
        osc.HSEState = RCC_HSE_OFF;
        osc.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
        osc.PLL.PLLMUL = RCC_PLL_MUL16;
        if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
            Error_Handler();
        }
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void gpio_init(void)
{
    GPIO_InitTypeDef g = {0};
    static const board_pin_t buttons[] = BOARD_BUTTONS;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* Siren output: set inactive level before switching to output. */
    HAL_GPIO_WritePin(SIREN_PORT, SIREN_PIN, SIREN_ACTIVE_HIGH ? GPIO_PIN_RESET : GPIO_PIN_SET);
    g.Pin = SIREN_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SIREN_PORT, &g);

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);   /* LED off */
    g.Pin = LED_PIN;
    HAL_GPIO_Init(LED_PORT, &g);

    for (size_t i = 0; i < sizeof buttons / sizeof buttons[0]; i++) {
        g.Pin = buttons[i].pin;
        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(buttons[i].port, &g);
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

void Error_Handler(void)
{
    /* Fail safe: siren off, blink LED, let the watchdog reset us. */
    __disable_irq();
    HAL_GPIO_WritePin(SIREN_PORT, SIREN_PIN, SIREN_ACTIVE_HIGH ? GPIO_PIN_RESET : GPIO_PIN_SET);
    for (;;) {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        for (volatile uint32_t i = 0; i < 300000U; i++) {
        }
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
