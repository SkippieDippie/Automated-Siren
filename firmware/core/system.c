#include "system.h"

#include "board.h"

void system_gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

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
}

/* 8 MHz HSE x9 -> 72 MHz SYSCLK, APB1 36 MHz, APB2 72 MHz. */
clock_source_t system_clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    clock_source_t src = CLOCK_HSE_72MHZ;

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        /* No crystal? Fall back to HSI/2 x16 = 64 MHz so the panel still works. */
        src = CLOCK_HSI_64MHZ;
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
    return src;
}

void Error_Handler(void)
{
    /* Fail safe: siren off, blink LED; the watchdog (if running) resets us. */
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
