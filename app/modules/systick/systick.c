#include "systick.h"

static volatile uint32_t s_tick_ms = 0;

/* 默认空回调, 工程可在别处重定义 */
__weak void Systick_OnTick(void)
{
}

void Systick_Init(void)
{
    /* HCLK 时钟源, 1ms 周期 */
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
    SysTick->LOAD = SystemCoreClock / 1000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
}

uint32_t Systick_GetTick(void)
{
    return s_tick_ms;
}

void Systick_DelayMs(uint32_t ms)
{
    uint32_t start = s_tick_ms;
    while ((s_tick_ms - start) < ms)
    {
    }
}

void SysTick_Handler(void)
{
    s_tick_ms++;
    Systick_OnTick();
}
