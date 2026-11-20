#ifndef __BOOT_API_H
#define __BOOT_API_H

/* ================================================================== *
 * Bootloader <-> APP 交互接口
 *
 * 使用方式: APP 工程将该头文件加入 include 路径,
 * 在需要进入升级模式时调用 Boot_RequestBootloader()。
 *
 * 原理:
 *   1. 将魔数及其反码写入两个备份寄存器
 *   2. 触发软件复位
 *   3. Bootloader 启动后检测到魔数, 清除并进入升级模式
 * ================================================================== */

#include "stm32f4xx.h"

/* APP 请求重入 Bootloader 的魔数 (双方约定) */
#define BOOT_REQUEST_MAGIC   0xDEADBEEF
#define BOOT_BKP_REG         RTC->BKP1R
#define BOOT_BKP_INV_REG     RTC->BKP2R

/* HardFault 循环保护计数器 (与 Bootloader 共享, RTC 备份域跨复位保持) */
#define BOOT_BKP_FAULT_REG   RTC->BKP4R

/* ---- 分区契约 (APP 与 BL 共享, 需保持一致) ---- */
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
    BOOT_BKP_INV_REG = ~BOOT_REQUEST_MAGIC;

    PWR->CR &= ~PWR_CR_DBP;

    __DSB();
    NVIC_SystemReset();
}

/**
 * @brief  上报 APP 致命异常并复位 (在内核异常处理器中调用, 不返回)
 *
 * BKP4R 计数 +1 后立即系统复位。Bootloader 启动时读取该计数, 连续
 * Fault 复位达到其配置上限 (boot_config.h BOOT_FAULT_LIMIT, 当前 3 次)
 * 后不再自动跳 APP, 停在升级模式等待修复, 避免 Fault-复位-Fault
 * 无限循环。Bootloader 自身的同名内核异常同样计入该计数。
 *
 * 计数只由两处清零: 升级成功提交后由 Bootloader 清零, 以及 APP 稳定
 * 运行后调用 Boot_ReportHealthy()。不要在 APP 启动早期调用后者。
 */
static inline void Boot_ReportFaultReset(void)
{
    /* 使能 PWR 时钟与备份域写访问 (HardFault 屏蔽低优先级中断,
     * 寄存器操作无需额外关中断) */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    if (BOOT_BKP_FAULT_REG < 0xFFFFu)
        BOOT_BKP_FAULT_REG++;

    __DSB();
    NVIC_SystemReset();
}

/**
 * @brief  上报 APP 已稳定运行, 清零连续 Fault 复位计数
 *
 * 调用时机: APP 初始化完成、确认运行正常之后 (演示 APP 为启动后 5 s)。
 * 崩溃循环中的固件反复复位, 到不了这个上报点, 计数才能连续累积到
 * Bootloader 上限并让它停在升级模式; 在启动早期调用会让保护完全失效。
 *
 * 不做上报的固件: 计数只在升级成功时清零, 偶发的多次崩溃会累积并
 * 最终停在升级模式。
 */
static inline void Boot_ReportHealthy(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    BOOT_BKP_FAULT_REG = 0;

    __DSB();
    PWR->CR &= ~PWR_CR_DBP;
}

#endif /* __BOOT_API_H */
