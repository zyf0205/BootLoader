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

uint8_t BootPolicy_AllowUpgrade(uint8_t app_valid,
                                uint16_t installed_ver,
                                uint16_t incoming_ver)
{
    /* 防降级: 已装固件完好时, 拒绝比其更旧的版本。
     * - APP 无效 (recovery/首次烧写): 放行任意版本, 保证可救砖
     * - 任一版本未知 (0, 文件名无 vX.Y 或元数据无版本): 放行,
     *   避免误拦无法解析版本的固件 */
    if (!app_valid || installed_ver == 0 || incoming_ver == 0)
        return 1;
    return (incoming_ver >= installed_ver) ? 1 : 0;
}
