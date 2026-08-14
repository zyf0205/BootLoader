#ifndef __BOOT_API_H
#define __BOOT_API_H

/* ================================================================== *
 * Bootloader <-> APP 交互接口 (v3.0)
 *
 * 使用方式: APP 工程将该头文件加入 include 路径,
 * 在需要进入升级模式时调用 Boot_RequestBootloader()。
 *
 * 原理:
 *   1. 将魔数写入备份寄存器 (软件复位不丢失, 上电复位清零)
 *   2. 触发软件复位
 *   3. Bootloader 启动后检测到魔数, 清除并进入升级模式
 * ================================================================== */

#include "stm32f4xx.h"

/* APP 请求重入 Bootloader 的魔数 (双方约定) */
#define BOOT_REQUEST_MAGIC   0xDEADBEEF
#define BOOT_BKP_REG         RTC->BKP1R

/* ---- Bootloader 固件信息 (APP 与 BL 共享, 需保持一致) ---- */
#define BOOT_BL_VERSION_MAJOR  3
#define BOOT_BL_VERSION_MINOR  0
#define BOOT_APP_BASE_ADDR     0x08004000    /* APP 起始地址 (Sector 1) */
#define BOOT_APP_MAX_SIZE      0x3C000       /* APP 最大 240KB */

/**
 * @brief  请求重入 Bootloader (永不返回, 直接复位)
 *
 * 注意: 调用前应确保外设处于安全状态 (如关闭中断),
 * 若希望复位后不丢数据, 可将数据写入备份域。
 */
static inline void Boot_RequestBootloader(void)
{
    __disable_irq();

    /* 使能 PWR 时钟与备份域写访问 */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    BOOT_BKP_REG = BOOT_REQUEST_MAGIC;

    PWR->CR &= ~PWR_CR_DBP;

    __DSB();
    NVIC_SystemReset();
}

#endif /* __BOOT_API_H */
