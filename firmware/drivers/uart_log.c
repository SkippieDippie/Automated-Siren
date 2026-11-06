#include "uart_log.h"

#include <stdarg.h>
#include <stdio.h>

#include "stm32f1xx_hal.h"

void uart_log_init(uint32_t baud)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    g.Pin = GPIO_PIN_9;               /* TX */
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    USART1->CR1 = 0;
    /* USART1 is on APB2. BRR = fck / baud, rounded. */
    USART1->BRR = (HAL_RCC_GetPCLK2Freq() + baud / 2U) / baud;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void put(char c)
{
    while (!(USART1->SR & USART_SR_TXE)) {
    }
    USART1->DR = (uint8_t)c;
}

void uart_log(const char *fmt, ...)
{
    char buf[160];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    for (const char *p = buf; *p; p++) {
        put(*p);
    }
    put('\r');
    put('\n');
}
