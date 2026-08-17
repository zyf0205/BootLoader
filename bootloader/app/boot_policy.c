#include "boot_policy.h"

boot_reason_t BootPolicy_Select(uint8_t app_requested,
                                uint8_t key_held,
                                uint8_t app_valid)
{
    if (app_requested)
        return BOOT_REASON_APP_REQUEST;
    if (key_held)
        return BOOT_REASON_KEY;
    if (!app_valid)
        return BOOT_REASON_INVALID_APP;
    return BOOT_REASON_NORMAL;
}
