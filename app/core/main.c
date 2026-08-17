#include "stm32f4xx.h"
#include "systick.h"
#include "led.h"
#include "flash_config.h"

#define LED_BLINK_INTERVAL_MS  50

void Systick_OnTick(void)
{
}

int main(void)
{
    uint32_t blink_tick;

    /* 向量表偏移: Bootloader 跳转到此, 中断必须使用 APP 向量表 */
    SCB->VTOR = APP_ADDR;

    Systick_Init();
    Led_Init();

    Led_Set(LED_ON);
    blink_tick = Systick_GetTick();

    while (1)
    {
        /* LED 心跳闪烁 */
        if ((Systick_GetTick() - blink_tick) >= LED_BLINK_INTERVAL_MS)
        {
            blink_tick = Systick_GetTick();
            Led_Toggle();
        }

    }
}
