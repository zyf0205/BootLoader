#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx.h"

void Key_Init(void);

/* 读取当前按键状态 (已消抖), 1 = 按下 */
uint8_t Key_Read(void);

/* 读取按下沿事件 (读取后自动清零), 1 = 刚按下 */
uint8_t Key_GetEvent(void);

/* 必须在 1ms 定时中断中调用 */
void Key_Tick(void);

#endif /* __KEY_H */
