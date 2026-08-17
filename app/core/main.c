#include "stm32f4xx.h"
#include "systick.h"
#include "led.h"
#include "key.h"
#include "boot_api.h"
#include "flash_config.h"

#define LED_BLINK_INTERVAL_MS  1500    /* APP 慢闪: 每 1.5 秒翻转一次 */
#define KEY_HOLD_ENTER_BL_MS   2000    /* 长按进入 Bootloader 时间 */

/* 1ms 定时回调: 驱动按键消抖 */
void Systick_OnTick(void)
{
    Key_Tick();
}

int main(void)
{
    uint32_t blink_tick;
    uint32_t press_tick = 0;

    /* 向量表偏移: Bootloader 跳转到此, 中断必须使用 APP 向量表 */
    SCB->VTOR = APP_ADDR;

    Systick_Init();
    Led_Init();
    Key_Init();

    Led_Set(LED_ON);
    blink_tick = Systick_GetTick();

    while (1)
    {
        /* 1. LED 心跳闪烁 */
        if ((Systick_GetTick() - blink_tick) >= LED_BLINK_INTERVAL_MS)
        {
            blink_tick = Systick_GetTick();
            Led_Toggle();
        }

        /* 2. 长按按键 -> 请求重入 Bootloader (备份寄存器 + 软复位) */
        if (Key_GetEvent())
        {
            press_tick = Systick_GetTick();
        }

        if (Key_Read() && (Systick_GetTick() - press_tick) >= KEY_HOLD_ENTER_BL_MS)
        {
            Boot_RequestBootloader();
        }
    }
}
