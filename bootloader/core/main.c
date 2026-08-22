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

/* 1ms 中断只做轻量计数与消抖。YMODEM 超时逻辑不在这里跑:
 * 协议状态机由主循环独占访问, 避免中断/主循环双上下文并发
 * 读写状态与在中断里执行 printf/发送等非重入回调 (v3.3 竞态修复) */
static volatile uint32_t s_ymodem_ticks = 0;

void Systick_OnTick(void)
{
    Led_Tick();
    Key_Tick();
    s_ymodem_ticks++;
}

/* 消费中断累积的 1ms 节拍, 在主循环上下文驱动 YMODEM 超时状态机 */
static void PumpYmodemTicks(void)
{
    uint32_t pending;

    __disable_irq();
    pending        = s_ymodem_ticks;
    s_ymodem_ticks = 0;
    __enable_irq();

    while (pending--)
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
    uint32_t fault_resets;

    /* ---- 1. 硬件初始化 ---- */
    Systick_Init();
    Led_Init();
    Key_Init();
    Flash_Init();
    Usart_Init();
    Updater_Init();

    PrintBanner();

    /* ---- 2. 确定性启动决策 ---- */
    fault_resets = Boot_PeekFaultResets();
    post_update   = Boot_TakePostUpdate();
    app_requested = Boot_TakeAppRequest();
    key_held      = (app_requested || post_update) ? 0 : Boot_IsUpdateKeyHeld();
    app_valid     = (app_requested || key_held) ? 0 : Flash_IsAppValid();
    reason        = BootPolicy_Select(app_requested, key_held, app_valid);

    /* HardFault 复位循环保护: 连续多次异常复位后不再自动跳 APP,
     * 停在升级模式等待修复 (升级入口本身即逃生通道) */
    if (fault_resets >= BOOT_FAULT_LIMIT)
    {
        printf("\r\nWarning: %lu HardFault resets detected, stay in recovery.\r\n",
               (unsigned long)fault_resets);
        reason = BOOT_REASON_INVALID_APP;
    }

    if (reason == BOOT_REASON_NORMAL)
    {
        printf("\r\nAPP verified. Jumping...\r\n");
        Boot_ClearFaultResets();
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
    Boot_ClearFaultResets();    /* 已稳定进入升级模式, 清除 Fault 计数 */
    Updater_Begin(0);

    /* ---- 3. 升级主循环 (完成后自动跳转 APP) ---- */
    while (1)
    {
        Updater_Process();
        PumpYmodemTicks();
    }
}
