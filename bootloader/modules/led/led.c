#include "led.h"
#include "board.h"

/* ---- 内部状态 ---- */
static led_state_t s_state = LED_OFF;
static uint32_t    s_tick  = 0;

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
    s_tick  = 0;

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

    case LED_BLINK_RAPID:
        if ((s_tick % 50) == 0)
            Led_Toggle();
        break;

    case LED_HEARTBEAT:
        if ((s_tick % 1000) == 0)
            Led_Write(1);
        else if ((s_tick % 1000) == 50)
            Led_Write(0);
        break;

    default:
        break;
    }
}
