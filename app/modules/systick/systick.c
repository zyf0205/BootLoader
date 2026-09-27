#include "systick.h"

/* ================================================================== *
 * Cortex-M SysTick 定时器封装: 提供 1ms 系统节拍
 *   - SysTick_Handler 每 1ms 触发一次: 计时 + 调用 Systick_OnTick
 *   - Systick_OnTick 为 weak 符号, 各工程 (bootloader/app) 重定义
 *     它挂接 LED 闪烁、按键消抖等周期任务, 不用改本文件
 * ================================================================== */

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

/* 忙等延时。用无符号减法比较, 即使 s_tick_ms 溢出回绕也正确 */
void Systick_DelayMs(uint32_t ms)
{
    uint32_t start = s_tick_ms;
    while ((s_tick_ms - start) < ms)
    {
    }
}

/* 1ms 中断入口: 计时并在中断上下文执行各工程的周期任务 */
void SysTick_Handler(void)
{
    s_tick_ms++;
    Systick_OnTick();
}
