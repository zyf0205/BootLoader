#ifndef __BOOT_POLICY_H
#define __BOOT_POLICY_H

#include <stdint.h>

typedef enum {
    BOOT_REASON_NORMAL = 0,
    BOOT_REASON_KEY,
    BOOT_REASON_APP_REQUEST,
    BOOT_REASON_INVALID_APP,
} boot_reason_t;

boot_reason_t BootPolicy_Select(uint8_t app_requested,
                                uint8_t key_held,
                                uint8_t app_valid);

/* 防降级策略: 返回 1 允许升级, 0 拒绝 (incoming 低于 installed)。
 * app_valid=0 或任一版本为 0 (未知) 时放行 */
uint8_t BootPolicy_AllowUpgrade(uint8_t app_valid,
                                uint16_t installed_ver,
                                uint16_t incoming_ver);

#endif /* __BOOT_POLICY_H */
