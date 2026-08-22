#ifndef __BOOT_H
#define __BOOT_H

#include "stm32f4xx.h"
#include "boot_policy.h"

/* ================================================================== *
 * 启动决策: 决定进入升级模式还是跳转 APP
 * ================================================================== */

/* 读取并清除 APP 的一次性升级请求 */
uint8_t Boot_TakeAppRequest(void);

/* 检测启动阶段是否持续按住升级按键 */
uint8_t Boot_IsUpdateKeyHeld(void);

/* 消费升级完成后的受控复位标志 */
uint8_t Boot_TakePostUpdate(void);

/* 标记升级完成并执行系统复位 (不返回) */
void Boot_ResetAfterUpdate(void);

/* 跳转到 APP (APP 无效时返回) */
void Boot_JumpToApp(void);

/* ---- HardFault 复位循环保护 (RTC BKP4R) ---- */

/* 查询连续 HardFault 复位次数 (不清零) */
uint32_t Boot_PeekFaultResets(void);

/* HardFault 处理中记录一次复位 (达到 BOOT_FAULT_LIMIT 后不再累加) */
void Boot_RecordFaultReset(void);

/* 正常启动/进入升级模式时清零计数 */
void Boot_ClearFaultResets(void);

#endif /* __BOOT_H */
