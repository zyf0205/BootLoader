#ifndef __USART_H
#define __USART_H

#include "stm32f4xx.h"

/* ================================================================== *
 * 控制台串口驱动 (USART1)
 *   RX: DMA2_Stream2 环形缓冲, 主循环轮询读取 (无需中断)
 *   TX: 阻塞发送, 忙等带超时兜底
 * ================================================================== */

void Usart_Init(void);

/* 阻塞发送单字节 (供 printf 重定向使用, 不推荐业务代码调用) */
void Usart_Putc(uint8_t ch);

/* RX 环形缓冲可读字节数 */
uint16_t Usart_RxAvailable(void);

/* 读取一个字节: 返回 0=成功, 1=无数据 */
uint8_t Usart_ReadByte(uint8_t *byte);

/* 清空接收缓冲 */
void Usart_FlushRx(void);

/* 读取并清除 RX 溢出标志: 1 = DMA 覆盖过未读数据, 缓冲不可信 */
uint8_t Usart_TakeRxOverflow(void);

/* 等待 TX 完成 (跳转 APP 前调用) */
void Usart_WaitTxIdle(void);

/* 关闭所有串口外设 (跳转 APP 前调用) */
void Usart_DeInit(void);

#endif /* __USART_H */
