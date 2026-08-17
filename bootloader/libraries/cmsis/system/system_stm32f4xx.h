#ifndef __SYSTEM_STM32F4XX_H
#define __SYSTEM_STM32F4XX_H

/* ================================================================== *
 * CMSIS System 初始化 - 仅支持 STM32F411CEU6
 *
 * 系统时钟: 96MHz (F411 上限 100MHz)
 *   HCLK  = 96MHz   (AHB)
 *   PCLK1 = 48MHz   (APB1)
 *   PCLK2 = 96MHz   (APB2)
 *
 * 时钟源可通过 CLOCK_USE_HSE 选择:
 *   0: HSI 16MHz -> PLL(M=16, N=192, P=2) = 96MHz  (默认, 无需晶振)
 *   1: HSE 12MHz -> PLL(M=12, N=192, P=2) = 96MHz  (按实际晶振改 PLL_M)
 * ================================================================== */

#include <stdint.h>

/* 时钟源选择 */
#define CLOCK_USE_HSE   1

#if CLOCK_USE_HSE
#define PLL_SOURCE_HSE  1
#define PLL_M           12
#else
#define PLL_SOURCE_HSE  0
#define PLL_M           16
#endif

#define PLL_N           192
#define PLL_P           2
#define PLL_Q           4

#define SYSTEM_CLOCK_HZ 96000000U

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);

#endif /* __SYSTEM_STM32F4XX_H */
