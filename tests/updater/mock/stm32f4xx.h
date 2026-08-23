#ifndef __STM32F4XX_MOCK_H
#define __STM32F4XX_MOCK_H

/* 主机测试桩: 仅提供 updater.c 等源文件所需的基础类型与 CMSIS 接口 */
#include <stdint.h>

void NVIC_SystemReset(void);

#endif
