#include "boot.h"
#include "boot_api.h"
#include "flash_config.h"
#include "usart.h"
#include "key.h"
#include "systick.h"

#define KEY_HOLD_MS    50   /* 开机按键保持检测时间 */

boot_reason_t Boot_GetReason(void)
{
    boot_reason_t reason = BOOT_REASON_POWER_ON;

    /* 1. 检测 APP 重入请求 (备份寄存器魔数, 读后即清) */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    if (BOOT_BKP_REG == BOOT_REQUEST_MAGIC)
    {
        BOOT_BKP_REG = 0;
        reason = BOOT_REASON_APP_REQ;
    }

    PWR->CR &= ~PWR_CR_DBP;

    /* 2. 按键保持检测 (消抖采样) */
    if (reason == BOOT_REASON_POWER_ON)
    {
        uint8_t pressed = 1;
        for (uint32_t i = 0; i < KEY_HOLD_MS; i++)
        {
            if (!Key_Read())
            {
                pressed = 0;
                break;
            }
            Systick_DelayMs(1);
        }

        if (pressed)
            reason = BOOT_REASON_KEY;
    }

    return reason;
}

void Boot_JumpToApp(void)
{
    uint32_t msp;
    uint32_t reset;

    /* 1. 关闭外设与全局中断, 还给 APP 一个干净的环境 */
    __disable_irq();
    Usart_DeInit();
    SysTick->CTRL = 0;

    /* 2. 校验 APP 向量表 (无效则返回, 由主循环处理) */
    msp   = *(volatile uint32_t *)APP_ADDR;
    reset = *(volatile uint32_t *)(APP_ADDR + 4);

    if (msp < SRAM_BASE_ADDR || msp > (SRAM_BASE_ADDR + SRAM_SIZE))
        return;
    if (reset < APP_ADDR || reset >= APP_END_ADDR || (reset & 1) == 0)
        return;

    /* 3. 复位时钟树, APP 的 SystemInit 会重新配置 */
    RCC_DeInit();

    /* 4. 重定向向量表并跳转 */
    SCB->VTOR = APP_ADDR;
    __set_MSP(msp);
    __set_CONTROL(0);       /* 线程模式 + MSP (清空 PSP 使用标志) */
    __ISB();

    ((void (*)(void))reset)();
}
