#ifndef __BOOT_H
#define __BOOT_H

#include "stm32f4xx.h"

/* ================================================================== *
 * 启动决策: 决定进入升级模式还是跳转 APP
 * ================================================================== */

typedef enum {
    BOOT_REASON_POWER_ON = 0,   /* 上电启动 */
    BOOT_REASON_KEY,            /* 按键触发 */
    BOOT_REASON_APP_REQ,        /* APP 请求重入 */
} boot_reason_t;

/* 检测启动原因 (备份寄存器魔数 + 按键) */
boot_reason_t Boot_GetReason(void);

/* 跳转到 APP (APP 无效时返回) */
void Boot_JumpToApp(void);

#endif /* __BOOT_H */
