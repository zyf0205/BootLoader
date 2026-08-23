#ifndef __LED_H
#define __LED_H

#include "stm32f4xx.h"

/* LED 显示状态 (由 1ms 定时驱动, 主循环无需关心) */
typedef enum {
    LED_OFF = 0,        /* 熄灭 */
    LED_ON,             /* 常亮 */
    LED_BLINK_SLOW,     /* 慢闪 1Hz   - 等待连接 */
    LED_BLINK_FAST,     /* 快闪 5Hz   - 正在接收数据 */
} led_state_t;

void Led_Init(void);
void Led_Set(led_state_t state);
void Led_Toggle(void);

/* 必须在 1ms 定时中断中调用 */
void Led_Tick(void);

#endif /* __LED_H */
