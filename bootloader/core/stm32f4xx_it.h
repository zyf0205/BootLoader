#ifndef __STM32F4XX_IT_H
#define __STM32F4XX_IT_H

/* Cortex-M4 异常处理函数 */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);

/* 外设中断处理函数分布在各自驱动中:
 *   SysTick_Handler        -> modules/systick/systick.c
 *   DMA2_Stream7_IRQHandler -> bootloader/drivers/usart/usart.c
 */

#endif /* __STM32F4XX_IT_H */
