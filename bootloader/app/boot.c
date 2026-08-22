#include "boot.h"
#include "boot_api.h"
#include "flash_config.h"
#include "usart.h"
#include "key.h"
#include "board.h"
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

    /* 引脚恢复复位默认态: RCC_DeInit 不会复位 GPIO 配置, 残留的
     * AF/输出配置可能干扰 APP 的引脚初始化。GPIO_DeInit 脉冲复位
     * 整个端口, BL 仅用过 GPIOA (LED/USART) 与 GPIOC (KEY) */
    GPIO_DeInit(LED_PORT);
    GPIO_DeInit(KEY_PORT);

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    /* 3. 复位时钟树并关闭外设时钟: RCC_DeInit 只复位 CR/CFGR 等
     *    配置寄存器, 不清除 AHB1/APB1/APB2 的外设时钟使能位,
     *    GPIO/DMA2/USART1/CRC/PWR 时钟需显式关闭。 */
    RCC_DeInit();
    RCC->AHB1ENR = 0;
    RCC->APB1ENR = 0;
    RCC->APB2ENR = 0;
    (void)RCC->AHB1ENR;   /* 读回, 确保时钟关闭已生效 */

    /* 4. 重定向向量表并跳转。Reset_Handler 应在中断开启状态运行。 */
    SCB->VTOR = APP_ADDR;
    __set_MSP(msp);
    __set_CONTROL(0);       /* 线程模式 + MSP (清空 PSP 使用标志) */
    __enable_irq();
    __DSB();
    __ISB();

    ((void (*)(void))reset)();
}

uint32_t Boot_PeekFaultResets(void)
{
    uint32_t count;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    count = RTC->BKP4R;

    PWR->CR &= ~PWR_CR_DBP;
    return count;
}

void Boot_RecordFaultReset(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    if (RTC->BKP4R < BOOT_FAULT_LIMIT)
        RTC->BKP4R++;
    __DSB();

    PWR->CR &= ~PWR_CR_DBP;
}

void Boot_ClearFaultResets(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    RTC->BKP4R = 0;

    PWR->CR &= ~PWR_CR_DBP;
}
