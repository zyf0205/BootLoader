/* ================================================================== *
 * APP 演示固件入口
 * 功能: LED 心跳 (1s 周期); 长按 USER 键 1.5s 通过 boot_api 请求
 * 重入 Bootloader 进入 YMODEM 升级模式 (保留的软件重入 API 演示)。
 * ================================================================== */

#include "stm32f4xx.h"
#include "systick.h"
#include "led.h"
#include "key.h"
#include "boot_api.h"
#include "flash_config.h"

#define LED_BLINK_INTERVAL_MS  1000    /* 心跳闪烁周期 */
#define REENTRY_HOLD_MS        1500    /* 长按进入升级模式的判定时间 */
#define REENTRY_BLINK_MS       100     /* 长按期间 LED 快闪提示 */

void Systick_OnTick(void)
{
    Led_Tick();
    Key_Tick();
}

int main(void)
{
    uint32_t blink_tick;
    uint32_t blink_interval = LED_BLINK_INTERVAL_MS;
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
        /* 长按 USER 键 (PC13) 1.5s -> 请求重入 Bootloader 升级模式:
         * 写 BKP 魔数后系统复位, Bootloader 检测到魔数进入 YMODEM。
         * 短按不动作, 防止误触; 长按期间 LED 快闪给出提示 */
        if (Key_GetEvent())
            press_tick = Systick_GetTick();
        if (!Key_Read())
            press_tick = 0;

        if (press_tick != 0 &&
            (Systick_GetTick() - press_tick) >= REENTRY_HOLD_MS)
        {
            Led_Set(LED_ON);
            Boot_RequestBootloader();   /* 写魔数并复位, 不返回 */
        }

        /* LED 心跳闪烁 (长按期间转为快闪提示) */
        blink_interval = (press_tick != 0) ? REENTRY_BLINK_MS
                                           : LED_BLINK_INTERVAL_MS;
        if ((Systick_GetTick() - blink_tick) >= blink_interval)
        {
            blink_tick = Systick_GetTick();
            Led_Toggle();
        }
    }
}
