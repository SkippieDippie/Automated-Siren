/*
 * Cortex-M3 exception handlers.
 */
#include "stm32f1xx_hal.h"
#include "system.h"

void NMI_Handler(void)        { }
void HardFault_Handler(void)  { Error_Handler(); }
void MemManage_Handler(void)  { Error_Handler(); }
void BusFault_Handler(void)   { Error_Handler(); }
void UsageFault_Handler(void) { Error_Handler(); }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }

void SysTick_Handler(void)
{
    HAL_IncTick();
}
