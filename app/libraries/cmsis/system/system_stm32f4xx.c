#include "stm32f4xx.h"
#include "system_stm32f4xx.h"

/* 当前系统时钟 (HCLK) 频率 */
uint32_t SystemCoreClock = SYSTEM_CLOCK_HZ;

/* ---- 主 PLL 配置 ---- */
static void SetSysClock(void)
{
    __IO uint32_t hse_ready = 0;
    __IO uint32_t timeout = 0;

    /* 1. 电源稳压器: Scale 1 模式 (F411 复位默认 Scale 2, 仅支持 84MHz) */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS_0;

    /* 2. AHB/APB 分频: HCLK=SYSCLK, PCLK1=HCLK/2, PCLK2=HCLK */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

#if PLL_SOURCE_HSE
    /* 3a. 使能 HSE 并等待就绪 */
    RCC->CR |= RCC_CR_HSEON;
    do
    {
        hse_ready = RCC->CR & RCC_CR_HSERDY;
        timeout++;
    } while (hse_ready == 0 && timeout < HSE_STARTUP_TIMEOUT);

    if (hse_ready == 0)
    {
        /* HSE 启动失败: 使用 HSI 兜底 (换 M=16) */
        RCC->PLLCFGR = 16 | (PLL_N << 6) | (((PLL_P >> 1) - 1) << 16) |
                       (RCC_PLLCFGR_PLLSRC_HSI) | (PLL_Q << 24);
        SystemCoreClock = SYSTEM_CLOCK_HZ;   /* HSI 16MHz, M=16 -> 同为 96MHz */
    }
    else
#endif
    {
        /* 3b. 配置 PLL */
        RCC->PLLCFGR = PLL_M | (PLL_N << 6) | (((PLL_P >> 1) - 1) << 16) |
#if PLL_SOURCE_HSE
                       (RCC_PLLCFGR_PLLSRC_HSE) |
#else
                       (RCC_PLLCFGR_PLLSRC_HSI) |
#endif
                       (PLL_Q << 24);
    }

    /* 4. 使能 PLL 并等待锁定 */
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0)
    {
    }

    /* 5. Flash: 预取 + 指令/数据缓存 + 3 个等待周期 (96MHz) */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN | FLASH_ACR_LATENCY_3WS;

    /* 6. 切换到 PLL 输出 */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }
}

void SystemInit(void)
{
    /* FPU: CP10/CP11 完全访问 */
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
#endif

    /* 复位 RCC 到默认状态 */
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0x00000000;
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->PLLCFGR = 0x24003010;
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CIR = 0x00000000;

    /* 配置系统时钟 */
    SetSysClock();

    /* 向量表位于 Flash 起始 */
    SCB->VTOR = FLASH_BASE;
}

void SystemCoreClockUpdate(void)
{
    /* 本工程仅使用固定 96MHz 配置 */
    SystemCoreClock = SYSTEM_CLOCK_HZ;
}
