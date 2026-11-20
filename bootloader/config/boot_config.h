#ifndef __BOOT_CONFIG_H
#define __BOOT_CONFIG_H

/* ================================================================== *
 * Bootloader 行为配置
 * ================================================================== */

/* 复位阶段持续按住升级按键的判定时间 */
#define BOOT_KEY_HOLD_MS        100

/* 升级成功后受控复位前的等待时间 (让用户看到结果提示) */
#define BOOT_JUMP_DELAY_MS      1000

/* 升级完成后的受控复位标志 (RTC BKP3R, 复位后跳过按键检测一次) */
#define BOOT_POST_UPDATE_MAGIC  0x504F5354  /* "POST" */

/* 连续 HardFault 复位达到该次数后, 不再自动跳 APP, 停在升级模式
 * (计数存 RTC BKP4R, 由升级成功提交与 APP 的 Boot_ReportHealthy()
 * 清零; 进入升级模式本身不清零, 形成"必须修复才解除"的闩锁) */
#define BOOT_FAULT_LIMIT        3

#endif /* __BOOT_CONFIG_H */
