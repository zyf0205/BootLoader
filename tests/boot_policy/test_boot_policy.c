#include "boot_policy.h"
#include <stdio.h>
#include <stdlib.h>

static void Expect(boot_reason_t actual,
                   boot_reason_t expected,
                   const char *scenario)
{
    if (actual != expected)
    {
        fprintf(stderr, "FAIL: %s (actual=%d expected=%d)\n",
                scenario, (int)actual, (int)expected);
        exit(1);
    }
}

static void ExpectAllow(uint8_t actual,
                        uint8_t expected,
                        const char *scenario)
{
    if (actual != expected)
    {
        fprintf(stderr, "FAIL: %s (actual=%u expected=%u)\n",
                scenario, (unsigned)actual, (unsigned)expected);
        exit(1);
    }
}

int main(void)
{
    Expect(BootPolicy_Select(0, 0, 1), BOOT_REASON_NORMAL,
           "valid APP boots normally");
    Expect(BootPolicy_Select(0, 0, 0), BOOT_REASON_INVALID_APP,
           "invalid APP enters recovery");
    Expect(BootPolicy_Select(0, 1, 1), BOOT_REASON_KEY,
           "held key forces update");
    Expect(BootPolicy_Select(1, 0, 1), BOOT_REASON_APP_REQUEST,
           "APP request forces update");
    Expect(BootPolicy_Select(1, 1, 0), BOOT_REASON_APP_REQUEST,
           "APP request has highest priority");
    Expect(BootPolicy_Select(0, 1, 0), BOOT_REASON_KEY,
           "key has priority over invalid APP");

    /* 防降级决策表 */
    ExpectAllow(BootPolicy_AllowUpgrade(1, 0x0102, 0x0103), 1,
                "newer version allowed");
    ExpectAllow(BootPolicy_AllowUpgrade(1, 0x0102, 0x0102), 1,
                "same version allowed (re-flash)");
    ExpectAllow(BootPolicy_AllowUpgrade(1, 0x0102, 0x0101), 0,
                "older version rejected");
    ExpectAllow(BootPolicy_AllowUpgrade(1, 0x02FF, 0x0300), 1,
                "version compare numeric not string");
    ExpectAllow(BootPolicy_AllowUpgrade(0, 0x0102, 0x0001), 1,
                "invalid APP allows any version (recovery escape)");
    ExpectAllow(BootPolicy_AllowUpgrade(1, 0x0102, 0), 1,
                "unknown incoming version allowed");
    ExpectAllow(BootPolicy_AllowUpgrade(1, 0, 0x0001), 1,
                "unknown installed version allowed");

    puts("boot policy tests passed");
    return 0;
}
