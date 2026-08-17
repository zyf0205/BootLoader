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

#endif /* __BOOT_POLICY_H */
