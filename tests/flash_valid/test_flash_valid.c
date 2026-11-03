/* Flash_IsAppValid / Flash_LoadMeta 主机单元测试
 *
 * 编译**真实的** bootloader/drivers/flash/flash.c, 仅替换:
 *   - StdPeriph Flash 接口 -> 一块模拟 Flash 缓冲区
 *   - 硬件 CRC32 外设      -> 软件 CRC-32/MPEG-2 (与 STM32 硬件算法一致)
 *
 * 覆盖启动校验的每个拒绝分支: 元数据魔数/大小、MSP 范围、Reset 向量
 * 范围与 Thumb 位、全固件 CRC32、元数据字段位翻转、旧 16B 格式兼容。
 * 这些是决定"能否启动"的最后一道门, 但此前只在实机上被间接验证。 */
#include <stdio.h>
#include <string.h>
#include "flash.h"
#include "crc32.h"

/* ============ 模拟 Flash ============ */

/* APP 区 + 元数据区 (元数据只用到前 20B) */
uint8_t sim_flash[0x3C000u + 128u];

static void FlashSimReset(void)
{
    memset(sim_flash, 0xFF, sizeof(sim_flash));
}

void FLASH_Unlock(void) {}
void FLASH_Lock(void) {}
void FLASH_ClearFlag(uint32_t flags) { (void)flags; }

FLASH_Status FLASH_EraseSector(uint32_t sector, VoltageRange_TypeDef voltage)
{
    (void)voltage;
    if (sector >= FLASH_Sector_1 && sector <= FLASH_Sector_5)
        memset((void *)APP_ADDR, 0xFF, APP_SIZE);
    else if (sector == FLASH_Sector_6)
        memset((void *)META_ADDR, 0xFF, 128);
    return FLASH_COMPLETE;
}

FLASH_Status FLASH_ProgramWord(uintptr_t addr, uint32_t data)
{
    /* 编程只能 1->0 */
    if ((*(volatile uint32_t *)addr & data) != data)
        return FLASH_ERROR_PGA;
    *(volatile uint32_t *)addr = data;
    return FLASH_COMPLETE;
}

/* ============ 软件 CRC-32/MPEG-2 (对齐 STM32 硬件语义) ============ */

static uint32_t s_crc;

void Crc32_Init(void) { s_crc = 0xFFFFFFFFu; }
void Crc32_Reset(void) { s_crc = 0xFFFFFFFFu; }

void Crc32_Accumulate(uint32_t word)
{
    uint32_t i;

    s_crc ^= word;
    for (i = 0; i < 32; i++)
    {
        if (s_crc & 0x80000000u)
            s_crc = (s_crc << 1) ^ 0x04C11DB7u;
        else
            s_crc <<= 1;
    }
}

uint32_t Crc32_Result(void) { return s_crc; }

/* ============ 测试基础设施 ============ */

static int test_failed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        test_failed = 1; \
    } \
} while (0)

/* 元数据自身 CRC: 与 flash.c 的 Meta_ComputeCrc 一致 (前 16 字节) */
static uint32_t MetaCrc(const fw_meta_t *m)
{
    const uint32_t *p = (const uint32_t *)m;
    uint32_t i;

    Crc32_Reset();
    for (i = 0; i < 4; i++)
        Crc32_Accumulate(p[i]);
    return Crc32_Result();
}

/* 构造一个"完好"的 APP + 元数据, 大小 size */
static void BuildValidApp(uint32_t size)
{
    fw_meta_t *meta = (fw_meta_t *)META_ADDR;
    uint32_t  *vec  = (uint32_t *)APP_ADDR;
    uint32_t   i;

    FlashSimReset();

    /* 向量表: MSP 在 SRAM 内, Reset 指向 APP 区且 Thumb 位为 1 */
    vec[0] = 0x2001FFF0u;                 /* 初始 MSP */
    vec[1] = (uint32_t)(APP_ADDR + 0x100u) | 1u;

    /* 其余固件内容 (避开向量表) */
    for (i = 8; i < size / 4; i++)
        vec[i] = 0x11110000u + i;

    meta->magic    = META_MAGIC;
    meta->size     = size;
    meta->crc32    = Flash_CalcAppCrc32(size);
    meta->version  = 0x0102;
    meta->reserved = 0;
    meta->meta_crc32 = MetaCrc(meta);
}

/* ============ 用例 ============ */

/* 1. 完好固件通过校验 */
static void test_valid_app(void)
{
    BuildValidApp(4096);
    CHECK(Flash_IsAppValid() == 1, "valid app accepted");

    fw_meta_t out;
    CHECK(Flash_LoadMeta(&out) == 1, "valid meta loaded");
    CHECK(out.version == 0x0102, "version preserved");

    printf("test_valid_app: PASS\n");
}

/* 2. 元数据 magic 无效 */
static void test_bad_magic(void)
{
    BuildValidApp(4096);
    ((fw_meta_t *)META_ADDR)->magic = 0;
    CHECK(Flash_IsAppValid() == 0, "cleared magic rejected");

    printf("test_bad_magic: PASS\n");
}

/* 3. MSP 越出 SRAM */
static void test_bad_msp(void)
{
    BuildValidApp(4096);

    ((uint32_t *)APP_ADDR)[0] = 0x10000000u;   /* 不在 SRAM */
    CHECK(Flash_IsAppValid() == 0, "MSP outside SRAM rejected");

    ((uint32_t *)APP_ADDR)[0] = 0x20020010u;   /* 超出 SRAM 上界 */
    CHECK(Flash_IsAppValid() == 0, "MSP above SRAM end rejected");

    printf("test_bad_msp: PASS\n");
}

/* 4. Reset 向量越界或 Thumb 位为 0 */
static void test_bad_reset_vector(void)
{
    BuildValidApp(4096);

    ((uint32_t *)APP_ADDR)[1] = (uint32_t)(APP_ADDR + APP_SIZE);  /* 恰好越界 */
    CHECK(Flash_IsAppValid() == 0, "reset at APP end rejected");

    ((uint32_t *)APP_ADDR)[1] = 0x08000001u;                      /* Bootloader 区 */
    CHECK(Flash_IsAppValid() == 0, "reset in BL region rejected");

    ((uint32_t *)APP_ADDR)[1] = (uint32_t)(APP_ADDR + 0x100u);    /* Thumb=0 */
    CHECK(Flash_IsAppValid() == 0, "reset without thumb bit rejected");

    printf("test_bad_reset_vector: PASS\n");
}

/* 5. 元数据 size 非法 */
static void test_bad_size(void)
{
    BuildValidApp(4096);

    ((fw_meta_t *)META_ADDR)->size = 0;
    CHECK(Flash_IsAppValid() == 0, "zero size rejected");

    ((fw_meta_t *)META_ADDR)->size = APP_SIZE + 4u;
    CHECK(Flash_IsAppValid() == 0, "oversized app rejected");

    printf("test_bad_size: PASS\n");
}

/* 6. 固件内容与 CRC32 不符 (半写/损坏) */
static void test_crc_mismatch(void)
{
    BuildValidApp(4096);

    ((uint32_t *)APP_ADDR)[100] ^= 0x00000001u;   /* 单比特翻转 */
    CHECK(Flash_IsAppValid() == 0, "corrupted app rejected");

    printf("test_crc_mismatch: PASS\n");
}

/* 7. 元数据字段位翻转 (meta_crc32 保护) */
static void test_meta_field_corruption(void)
{
    fw_meta_t *meta = (fw_meta_t *)META_ADDR;

    BuildValidApp(4096);

    meta->version ^= 0x0001u;    /* 防降级依赖 version, 必须被拦截 */
    CHECK(Flash_LoadMeta(&(fw_meta_t){0}) == 0, "flipped version rejected");
    CHECK(Flash_IsAppValid() == 0, "flipped meta invalidates app");

    printf("test_meta_field_corruption: PASS\n");
}

/* 8. 旧 16B 格式 (meta_crc32 = 擦除态) 兼容放行 */
static void test_legacy_meta_accepted(void)
{
    fw_meta_t *meta = (fw_meta_t *)META_ADDR;

    BuildValidApp(4096);

    meta->meta_crc32 = 0xFFFFFFFFu;   /* 旧格式没有该字段 */
    CHECK(Flash_LoadMeta(&(fw_meta_t){0}) == 1, "legacy meta accepted");
    CHECK(Flash_IsAppValid() == 1, "legacy meta app valid");

    printf("test_legacy_meta_accepted: PASS\n");
}

/* 9. CalcAppCrc32 参数边界 */
static void test_crc_param_guard(void)
{
    CHECK(Flash_CalcAppCrc32(0) == 0, "zero size crc returns 0");
    CHECK(Flash_CalcAppCrc32(APP_SIZE + 4u) == 0, "oversized crc returns 0");

    printf("test_crc_param_guard: PASS\n");
}

/* 10. 尾字节不足 4 字节的 CRC 计算 (补 0xFF) 与校验一致 */
static void test_tail_padding(void)
{
    BuildValidApp(4095);   /* 非 4 字节对齐 */
    CHECK(Flash_IsAppValid() == 1, "unaligned size verified consistently");

    printf("test_tail_padding: PASS\n");
}

int main(void)
{
    test_valid_app();
    test_bad_magic();
    test_bad_msp();
    test_bad_reset_vector();
    test_bad_size();
    test_crc_mismatch();
    test_meta_field_corruption();
    test_legacy_meta_accepted();
    test_crc_param_guard();
    test_tail_padding();

    if (test_failed)
    {
        printf("\n===== SOME TESTS FAILED =====\n");
        return 1;
    }
    printf("\n===== ALL TESTS PASSED =====\n");
    return 0;
}
