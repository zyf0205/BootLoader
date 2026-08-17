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
    printf("  Protocol : YMODEM (CRC16, SOH/STX)\r\n");
    printf("  Build    : %s %s\r\n", __DATE__, __TIME__);
    printf("============================================\r\n");
}

int main(void)
{
    boot_reason_t reason;
    uint8_t app_requested;
    uint8_t post_update;
    uint8_t key_held;
    uint8_t app_valid;

    /* ---- 1. 硬件初始化 ---- */
    Systick_Init();
    Led_Init();
    Key_Init();
    Flash_Init();
    Usart_Init();
    Updater_Init();

    PrintBanner();

    /* ---- 2. 确定性启动决策 ---- */
    post_update   = Boot_TakePostUpdate();
    app_requested = Boot_TakeAppRequest();
    key_held      = (app_requested || post_update) ? 0 : Boot_IsUpdateKeyHeld();
    app_valid     = (app_requested || key_held) ? 0 : Flash_IsAppValid();
    reason        = BootPolicy_Select(app_requested, key_held, app_valid);

    if (reason == BOOT_REASON_NORMAL)
    {
        printf("\r\nAPP verified. Jumping...\r\n");
        Boot_JumpToApp();
        printf("APP jump failed. Enter recovery mode.\r\n");
    }

    switch (reason)
    {
    case BOOT_REASON_KEY:
        printf("\r\nUpdate key held during reset.\r\n");
        break;
    case BOOT_REASON_APP_REQUEST:
        printf("\r\nUpdate requested by APP.\r\n");
        break;
    case BOOT_REASON_INVALID_APP:
        printf("\r\nAPP is missing or invalid.\r\n");
        break;
    default:
        printf("\r\nRecovery mode.\r\n");
        break;
    }

    printf("Waiting for YMODEM (send App.bin)...\r\n\r\n");
    Led_Set(LED_BLINK_SLOW);
    Updater_Begin(0);

    /* ---- 3. 升级主循环 (完成后自动跳转 APP) ---- */
    while (1)
    {
        Updater_Process();
    }
}
