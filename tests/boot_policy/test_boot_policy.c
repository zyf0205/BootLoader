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

    puts("boot policy tests passed");
    return 0;
}
