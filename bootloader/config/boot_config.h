#ifndef __BOOT_CONFIG_H
#define __BOOT_CONFIG_H

/* ================================================================== *
 * Bootloader 行为配置
 * ================================================================== */

/* 上电后等待 YMODEM 握手的时间窗口 (APP 有效时) */
#define BOOT_WAIT_TIMEOUT_MS    5000

/* 升级成功后跳转 APP 前的等待时间 (让用户看到结果提示) */
#define BOOT_JUMP_DELAY_MS      1000

#endif /* __BOOT_CONFIG_H */
