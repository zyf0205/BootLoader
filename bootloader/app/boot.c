#include "boot.h"
#include "boot_api.h"
#include "flash_config.h"
#include "usart.h"
#include "key.h"
#include "systick.h"
#include "boot_config.h"

uint8_t Boot_TakeAppRequest(void)
{
    uint8_t requested = 0;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    if (BOOT_BKP_REG == BOOT_REQUEST_MAGIC &&
        BOOT_BKP_INV_REG == ~BOOT_REQUEST_MAGIC)
    {
        requested = 1;
    }

    BOOT_BKP_REG = 0;
    BOOT_BKP_INV_REG = 0;

    PWR->CR &= ~PWR_CR_DBP;
    return requested;
}

uint8_t Boot_IsUpdateKeyHeld(void)
{
    uint32_t i;

    /* 必须在整个检测窗口保持按下，避免上电毛刺误触发。 */
    for (i = 0; i < BOOT_KEY_HOLD_MS; i++)
    {
        if (!Key_Read())
            return 0;
        Systick_DelayMs(1);
    }
    return 1;
}

uint8_t Boot_TakePostUpdate(void)
{
    uint8_t post_update = 0;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    if (RTC->BKP3R == BOOT_POST_UPDATE_MAGIC)
        post_update = 1;
    RTC->BKP3R = 0;

    PWR->CR &= ~PWR_CR_DBP;
    return post_update;
}

void Boot_ResetAfterUpdate(void)
{
    Usart_WaitTxIdle();

    __disable_irq();
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
    RTC->BKP3R = BOOT_POST_UPDATE_MAGIC;
    PWR->CR &= ~PWR_CR_DBP;
    __DSB();
    NVIC_SystemReset();

    while (1)
    {
    }
}

void Boot_JumpToApp(void)
{
    uint32_t msp;
    uint32_t reset;
    uint32_t i;

    /* 1. 校验 APP 向量表, 避免校验失败后留下已关闭的外设/中断。 */
    msp   = *(volatile uint32_t *)APP_ADDR;
    reset = *(volatile uint32_t *)(APP_ADDR + 4);

    if (msp < SRAM_BASE_ADDR || msp > (SRAM_BASE_ADDR + SRAM_SIZE))
        return;
    if (reset < APP_ADDR || reset >= APP_END_ADDR || (reset & 1) == 0)
        return;

    /* 2. 等日志发送完成后关闭外设, 还给 APP 一个干净的环境。 */
    Usart_WaitTxIdle();
    __disable_irq();
    Usart_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    /* 3. 复位时钟树, APP 的 SystemInit 会重新配置。 */
    RCC_DeInit();

    /* 4. 重定向向量表并跳转。Reset_Handler 应在中断开启状态运行。 */
    SCB->VTOR = APP_ADDR;
    __set_MSP(msp);
    __set_CONTROL(0);       /* 线程模式 + MSP (清空 PSP 使用标志) */
    __enable_irq();
    __DSB();
    __ISB();

    ((void (*)(void))reset)();
}
