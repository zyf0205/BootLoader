#ifndef __SYSTICK_H
#define __SYSTICK_H

#include "stm32f4xx.h"

/* 初始化 1ms 系统节拍 (基于 SystemCoreClock) */
void Systick_Init(void);

/* 获取开机以来累计毫秒数 */
uint32_t Systick_GetTick(void);

/* 阻塞延时 */
void Systick_DelayMs(uint32_t ms);

/* 1ms 定时回调, 各工程按需重定义 */
void Systick_OnTick(void);

#endif /* __SYSTICK_H */
