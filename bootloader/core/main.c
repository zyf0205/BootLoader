#include "stm32f4xx.h"
#include "systick.h"
#include "led.h"
#include "key.h"
#include "usart.h"
#include "flash.h"
#include "ymodem.h"
#include "boot.h"
#include "updater.h"
#include "boot_config.h"
#include "version.h"
#include <stdio.h>

/* 1ms 定时回调: 驱动 LED/按键消抖/YMODEM 超时 */
void Systick_OnTick(void)
{
    Led_Tick();
    Key_Tick();
    ymodem_tick();
}

static void PrintBanner(void)
{
    printf("\r\n============================================\r\n");
    printf("  STM32F411 Bootloader v%s\r\n", BL_VERSION_STR);
    printf("  Protocol : YMODEM (CRC16, 128B/1K packet)\r\n");
    printf("  Build    : %s %s\r\n", __DATE__, __TIME__);
    printf("============================================\r\n");
}

int main(void)
{
    boot_reason_t reason;

    /* ---- 1. 硬件初始化 ---- */
    Systick_Init();
    Led_Init();
    Key_Init();
    Flash_Init();
    Usart_Init();
    Updater_Init();

    PrintBanner();

    /* ---- 2. 启动决策 ---- */
    reason = Boot_GetReason();

    if (reason == BOOT_REASON_POWER_ON && Flash_IsAppValid())
    {
        /* APP 有效: 打开升级窗口, 超时未收到 YMODEM 则跳转 APP */
        printf("\r\nAPP is valid.\r\n");
        printf("Waiting %d s for YMODEM transfer...\r\n",
               BOOT_WAIT_TIMEOUT_MS / 1000);
        printf("(Hold KEY and press RESET to force update)\r\n\r\n");

        Led_Set(LED_BLINK_SLOW);
        Updater_Begin(BOOT_WAIT_TIMEOUT_MS);

        while (!Updater_Finished())
        {
            Updater_Process();
        }

        if (Updater_State() == UPDATER_TIMEOUT)
        {
            printf("\r\nNo transfer, jumping to APP...\r\n");
            Boot_JumpToApp();
        }
        /* SUCCESS 状态继续向下, 由主循环完成跳转 */
    }
    else
    {
        /* 强制进入升级模式: 按键 / APP 请求 / 无有效 APP */
        switch (reason)
        {
        case BOOT_REASON_KEY:
            printf("\r\nKEY pressed, enter update mode.\r\n\r\n");
            break;
        case BOOT_REASON_APP_REQ:
            printf("\r\nUpdate requested by APP.\r\n\r\n");
            break;
        default:
            printf("\r\nNo valid APP, enter update mode.\r\n\r\n");
            break;
        }

        Led_Set(LED_BLINK_SLOW);
        Updater_Begin(0);   /* 无限等待 */
    }

    /* ---- 3. 升级主循环 (完成后自动跳转 APP) ---- */
    while (1)
    {
        Updater_Process();
    }
}
