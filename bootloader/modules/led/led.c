#include "led.h"
#include "board.h"

/* ================================================================== *
 * 状态 LED 驱动: 设置目标状态后由 1ms 的 Led_Tick() 自动维持闪烁,
 * 业务代码只调 Led_Set(), 不必自己管翻转时序。
 * 极性 (低电平点亮) 由 board.h 的 LED_ACTIVE_LOW 统一适配。
 * ================================================================== */

/* ---- 内部状态 ---- */
static led_state_t s_state = LED_OFF;
static uint32_t    s_tick  = 0;    /* 当前状态持续的毫秒数 */

/* 写引脚电平 (内部处理低电平点亮极性) */
static void Led_Write(uint8_t on)
{
#if LED_ACTIVE_LOW
    on = !on;
#endif
    if (on)
        GPIO_SetBits(LED_PORT, LED_PIN);
    else
        GPIO_ResetBits(LED_PORT, LED_PIN);
}

void Led_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(LED_CLK, ENABLE);

    gpio.GPIO_Pin   = LED_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(LED_PORT, &gpio);

    Led_Set(LED_OFF);
}

void Led_Set(led_state_t state)
{
    s_state = state;
    s_tick  = 0;    /* 重新计时, 让闪烁相位从头开始 */

    switch (state)
    {
    case LED_OFF:  Led_Write(0); break;
    case LED_ON:   Led_Write(1); break;
    default:       break;  /* 闪烁由 Led_Tick 驱动 */
    }
}

void Led_Toggle(void)
{
    GPIO_ToggleBits(LED_PORT, LED_PIN);
}

/* 每 1ms 由定时中断调用一次, 按当前状态维持闪烁节奏:
 * 慢闪每 500ms 翻转一次 (周期 1s = 1Hz), 快闪每 100ms (5Hz) */
void Led_Tick(void)
{
    s_tick++;

    switch (s_state)
    {
    case LED_BLINK_SLOW:
        if ((s_tick % 500) == 0)
            Led_Toggle();
        break;

    case LED_BLINK_FAST:
        if ((s_tick % 100) == 0)
            Led_Toggle();
        break;

    default:
        break;
    }
}
