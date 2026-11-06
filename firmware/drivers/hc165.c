#include "hc165.h"

/* 74HC165 at 3.3 V needs ~25 ns pulse widths; a few cycles at 72 MHz
 * plus the GPIO write latency is plenty, this just adds margin. */
static inline void short_delay(void)
{
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
}

void hc165_init(const hc165_t *hc)
{
    GPIO_InitTypeDef g = {0};

    HAL_GPIO_WritePin(hc->load_port, hc->load_pin, GPIO_PIN_SET);  /* idle: shift mode */
    HAL_GPIO_WritePin(hc->clk_port, hc->clk_pin, GPIO_PIN_RESET);

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = hc->load_pin;
    HAL_GPIO_Init(hc->load_port, &g);
    g.Pin = hc->clk_pin;
    HAL_GPIO_Init(hc->clk_port, &g);

    /* Pull-up: a disconnected chain reads all-high = nothing pressed. */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Pin = hc->data_pin;
    HAL_GPIO_Init(hc->data_port, &g);
}

uint32_t hc165_read(const hc165_t *hc)
{
    uint32_t result = 0;

    /* Latch all parallel inputs. */
    HAL_GPIO_WritePin(hc->load_port, hc->load_pin, GPIO_PIN_RESET);
    short_delay();
    HAL_GPIO_WritePin(hc->load_port, hc->load_pin, GPIO_PIN_SET);
    short_delay();

    /* QH now shows input H of chip 0. Each rising clock edge shifts the
     * next input (G, F, ... A, then H of chip 1, ...) onto QH. */
    for (uint8_t c = 0; c < hc->chips; c++) {
        uint32_t byte = 0;
        for (uint8_t i = 0; i < 8; i++) {
            byte = (byte << 1) |
                   (HAL_GPIO_ReadPin(hc->data_port, hc->data_pin) == GPIO_PIN_SET ? 1U : 0U);
            HAL_GPIO_WritePin(hc->clk_port, hc->clk_pin, GPIO_PIN_SET);
            short_delay();
            HAL_GPIO_WritePin(hc->clk_port, hc->clk_pin, GPIO_PIN_RESET);
            short_delay();
        }
        result |= byte << (8U * c);   /* first bit read (H) lands on bit 7 */
    }
    return result;
}
