#ifndef __BOOT_CONFIG_H
#define __BOOT_CONFIG_H

/* ================================================================== *
 * Bootloader 行为配置
 * ================================================================== */

/* 复位阶段持续按住升级按键的判定时间 */
#define BOOT_KEY_HOLD_MS        100

/* 升级成功后跳转 APP 前的等待时间 (让用户看到结果提示) */
#define BOOT_JUMP_DELAY_MS      1000

/* 升级完成后的受控复位标志 (RTC BKP3R, 复位后跳过按键检测一次) */
#define BOOT_POST_UPDATE_MAGIC  0x504F5354  /* "POST" */

#endif /* __BOOT_CONFIG_H */
