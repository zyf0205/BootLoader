/* updater 掉电安全序列主机单元测试
 *
 * 编译真实的 updater.c + ymodem.c + boot_policy.c, 用模拟 Flash 替换驱动:
 *   1. 记录全部 Flash 操作顺序 (失效 -> 擦除 -> 写入 -> 提交)
 *   2. 模拟 Flash 物理特性: 只能 1->0、APP 区范围检查
 *   3. 每次操作后断言断电不变量: 元数据有效 => 固件已完整写入
 *      (等价于在每一个可能的断电时刻检查系统安全)
 * 配合失败注入 (元数据写失败 / 传输截断 / 防降级拒绝) 验证
 * "任何时刻断电, 不运行未验证固件"。 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "updater.h"
#include "ymodem.h"
#include "crc16.h"
#include "flash.h"
#include "led.h"
#include "usart.h"
#include "systick.h"
#include "flash_config.h"

/* ============ 测试基础设施 ============ */

static int test_failed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        test_failed = 1; \
    } \
} while (0)

/* 桩侧共享状态 (在 SimReset 中清零) */
static uint32_t tick_ms;
static int      reset_called;
static int      flushrx_count;
static uint8_t  s_overflow_once;
static led_state_t s_last_led;

/* ============ 模拟 Flash ============ */

/* 操作日志: 验证调用顺序 */
enum {
    OP_INVALIDATE, OP_ERASE, OP_WRITE, OP_CALCCRC, OP_SAVEMETA,
};
static struct {
    int      op;
    uint32_t a, b;
} s_log[512];
static int s_log_n = 0;

static void LogOp(int op, uint32_t a, uint32_t b)
{
    if (s_log_n < (int)(sizeof(s_log) / sizeof(s_log[0])))
    {
        s_log[s_log_n].op = op;
        s_log[s_log_n].a  = a;
        s_log[s_log_n].b  = b;
        s_log_n++;
    }
}

static int LogFind(int op, int from)
{
    int i;
    for (i = from; i < s_log_n; i++)
        if (s_log[i].op == op)
            return i;
    return -1;
}

static uint8_t *s_app;              /* APP 区镜像 (擦除态 0xFF) */
static fw_meta_t s_meta;            /* 元数据镜像 (magic=0 表示无效) */
static uint32_t s_watermark;        /* APP 区已写入高水位 (相对 APP_ADDR) */
static uint32_t s_sim_crc;          /* CalcAppCrc32 最近返回值 */
static uint8_t  s_fail_save_meta;   /* 失败注入: SaveMeta 返回错误 */
static uint8_t  s_load_valid;       /* LoadMeta 返回已装固件 (防降级用) */
static fw_meta_t s_load_content;
static int      s_invariant_hits;   /* 不变量实际检查次数 */
static int      s_invariant_fails;

/* 断电不变量: 元数据 magic 有效 => APP 已写入量不少于元数据声明大小。
 * 在每一次 Flash 操作后检查 —— 任何时刻断电都满足安全性 */
static void CheckInvariant(void)
{
    s_invariant_hits++;
    if (s_meta.magic == META_MAGIC && s_watermark < s_meta.size)
    {
        printf("FAIL: invariant broken (watermark=%lu meta.size=%lu)\n",
               (unsigned long)s_watermark, (unsigned long)s_meta.size);
        s_invariant_fails++;
        test_failed = 1;
    }
}

/* 模拟初始状态: 可选预装一个有效固件 (用于防降级测试) */
static void SimReset(int installed)
{
    if (s_app == NULL)
    {
        s_app = malloc(APP_SIZE);
        if (s_app == NULL)
        {
            printf("FAIL: out of memory\n");
            exit(1);
        }
    }
    memset(s_app, 0xFF, APP_SIZE);
    memset(&s_meta, 0, sizeof(s_meta));
    memset(&s_load_content, 0, sizeof(s_load_content));
    s_watermark       = 0;
    s_log_n           = 0;
    s_fail_save_meta  = 0;
    s_load_valid      = 0;
    s_invariant_hits  = 0;
    s_invariant_fails = 0;
    reset_called      = 0;
    flushrx_count     = 0;
    tick_ms           = 0;
    s_overflow_once   = 0;

    if (installed)
    {
        /* 预装 v1.5 固件 4096B: 元数据与 APP 内容一致 */
        s_meta.magic   = META_MAGIC;
        s_meta.size    = 4096;
        s_meta.version = 0x0105;
        s_watermark    = 4096;
        s_load_valid   = 1;
        s_load_content = s_meta;
    }
}

void Flash_Init(void) {}

flash_err_t Flash_EraseApp(void)
{
    LogOp(OP_ERASE, 0, 0);
    memset(s_app, 0xFF, APP_SIZE);
    s_watermark = 0;
    CheckInvariant();
    return FLASH_OK;
}

flash_err_t Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    LogOp(OP_WRITE, addr - APP_ADDR, len);
    if (addr < APP_ADDR || (addr + len) > APP_END_ADDR)
        return FLASH_ERR_PARAM;

    /* Flash 物理特性: 编程只能 1->0 */
    for (i = 0; i < len; i++)
    {
        if ((s_app[addr - APP_ADDR + i] & buf[i]) != buf[i])
        {
            printf("FAIL: flash physics violated at +%lu\n",
                   (unsigned long)(addr - APP_ADDR + i));
            test_failed = 1;
            return FLASH_ERR_WRITE;
        }
    }
    memcpy(&s_app[addr - APP_ADDR], buf, len);
    if ((addr - APP_ADDR + len) > s_watermark)
        s_watermark = addr - APP_ADDR + len;
    CheckInvariant();
    return FLASH_OK;
}

uint32_t Flash_CalcAppCrc32(uint32_t size)
{
    uint32_t i, crc = 0xFFFFFFFF;

    LogOp(OP_CALCCRC, size, 0);
    for (i = 0; i < size; i++)
        crc = (crc << 1 | crc >> 31) ^ s_app[i];   /* 确定性即可, 不求标准算法 */
    s_sim_crc = crc;
    return crc;
}

uint8_t Flash_IsAppValid(void)
{
    return s_meta.magic == META_MAGIC;
}

flash_err_t Flash_InvalidateMeta(void)
{
    LogOp(OP_INVALIDATE, 0, 0);
    s_meta.magic = 0;      /* 写 0 清 magic, 只能 1->0, 必然成功 */
    CheckInvariant();
    return FLASH_OK;
}

flash_err_t Flash_SaveMeta(const fw_meta_t *meta)
{
    LogOp(OP_SAVEMETA, meta->size, meta->version);
    if (s_fail_save_meta)
        return FLASH_ERR_ERASE;   /* 失败注入: 元数据未提交 */

    s_meta = *meta;       /* 真实实现中 magic 最后写入 */
    CheckInvariant();
    return FLASH_OK;
}

uint8_t Flash_LoadMeta(fw_meta_t *meta)
{
    if (s_load_valid)
    {
        *meta = s_load_content;
        return 1;
    }
    return 0;
}

/* ============ 其他硬件桩 ============ */

void Usart_Putc(uint8_t ch) { (void)ch; }
void Usart_FlushRx(void)    { flushrx_count++; }

uint16_t Usart_RxAvailable(void) { return 0; }
uint8_t Usart_ReadByte(uint8_t *byte) { (void)byte; return 1; }
uint8_t Usart_TakeRxOverflow(void)
{
    uint8_t f = s_overflow_once;
    s_overflow_once = 0;
    return f;
}

void Led_Set(led_state_t state) { s_last_led = state; }

uint32_t Systick_GetTick(void) { return tick_ms; }

void Boot_ResetAfterUpdate(void) { reset_called = 1; }
void Boot_JumpToApp(void)        { }
void NVIC_SystemReset(void)      { }   /* TIMEOUT 防御路径, 用例不触达 */

/* 模拟主循环: Updater_Process 每次调用只推进一步状态机
 * (WAITING -> RECEIVING -> Finalize), 连续泵送直至流程走完 */
static void PumpUpdater(int times)
{
    int i;
    for (i = 0; i < times; i++)
        Updater_Process();
}

/* ============ YMODEM 发送端模拟 (与 test_ymodem.c 同型) ============ */

static void tx_raw(uint8_t soh_stx, uint8_t seq,
                   const uint8_t *data, uint16_t len)
{
    uint16_t crc = Crc16_Calc(data, len);
    uint8_t  b;
    int      i;

    b = soh_stx;          ymodem_feed(b);
    b = seq;              ymodem_feed(b);
    b = (uint8_t)~seq;    ymodem_feed(b);
    for (i = 0; i < len; i++)
        ymodem_feed(data[i]);
    b = (uint8_t)(crc >> 8); ymodem_feed(b);
    b = (uint8_t)crc;        ymodem_feed(b);
}

static void tx_header(const char *name, uint32_t size)
{
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    snprintf((char *)data, 128, "%s%c%u", name, '\0', (unsigned)size);
    tx_raw(0x01, 0x00, data, 128);
}

static void tx_data(uint8_t seq, const uint8_t *data, uint16_t len)
{
    uint8_t buf[128];
    memset(buf, 0x1A, sizeof(buf));
    memcpy(buf, data, len);
    tx_raw(0x01, seq, buf, 128);
}

static void tx_eot(void) { ymodem_feed(0x04); }

static void tx_empty_header(void)
{
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    tx_raw(0x01, 0x00, data, 128);
}

/* 发送固件全部数据包并完成 EOT 握手 */
static void tx_firmware(const uint8_t *fw, uint32_t size)
{
    uint32_t off = 0;
    uint8_t  seq = 1;

    while (off < size)
    {
        uint16_t len = (uint16_t)((size - off) > 128 ? 128 : (size - off));
        tx_data(seq++, &fw[off], len);
        off += len;
    }
    tx_eot();
    tx_eot();
    tx_empty_header();
}

/* ============ 用例 ============ */

/* 1. 完整升级: 操作顺序 + 不变量 + 元数据内容 + 回读数据 */
static void test_full_update_sequence(void)
{
    static uint8_t fw[5000];
    int i, idx_inv, idx_erase, idx_write, idx_save;

    SimReset(0);
    for (i = 0; i < (int)sizeof(fw); i++)
        fw[i] = (uint8_t)(i * 31 + 7);

    Updater_Init();
    Updater_Begin(0);

    tx_header("fw_v1.4.bin", sizeof(fw));
    tx_firmware(fw, sizeof(fw));
    PumpUpdater(3);                          /* RECEIVING -> Finalize -> SUCCESS */

    /* 顺序: 失效 -> 擦除 -> 写入 -> 提交 */
    idx_inv   = LogFind(OP_INVALIDATE, 0);
    idx_erase = LogFind(OP_ERASE, 0);
    idx_write = LogFind(OP_WRITE, 0);
    idx_save  = LogFind(OP_SAVEMETA, 0);
    CHECK(idx_inv >= 0, "invalidate happened");
    CHECK(idx_erase > idx_inv, "invalidate BEFORE erase");
    CHECK(idx_write > idx_erase, "erase BEFORE write");
    CHECK(idx_save > idx_write, "all writes BEFORE meta commit");

    /* 元数据内容 */
    CHECK(s_meta.magic == META_MAGIC, "meta magic committed");
    CHECK(s_meta.size == sizeof(fw), "meta size");
    CHECK(s_meta.version == 0x0104, "meta version from filename");
    CHECK(s_meta.crc32 == s_sim_crc, "meta crc from CalcAppCrc32");

    /* 数据回读一致, 尾部保持擦除态 */
    CHECK(memcmp(s_app, fw, sizeof(fw)) == 0, "flash content matches");
    CHECK(s_app[sizeof(fw)] == 0xFF, "padding not written");

    /* 每一步操作后不变量均成立 */
    CHECK(s_invariant_fails == 0, "power-fail invariant holds at every step");
    CHECK(s_invariant_hits >= 40, "invariant actually checked");

    /* 延时到点后受控复位 */
    tick_ms += 1001;
    PumpUpdater(1);
    CHECK(reset_called == 1, "Boot_ResetAfterUpdate after delay");
    CHECK(s_last_led == LED_ON, "LED solid on success");

    printf("test_full_update_sequence: PASS (%lu B, invariant checked %d times)\n",
           (unsigned long)sizeof(fw), s_invariant_hits);
}

/* 2. 防降级: 已装 v1.5, 发送 v1.2 被拒, 全程不碰 APP 区 */
static void test_downgrade_rejected(void)
{
    uint8_t fw[128];

    SimReset(1 /* installed v1.5 */);
    memset(fw, 0, sizeof(fw));

    Updater_Init();
    Updater_Begin(0);

    tx_header("fw_v1.2.bin", sizeof(fw));
    PumpUpdater(2);        /* ABORTED -> Restart */

    CHECK(LogFind(OP_INVALIDATE, 0) < 0, "no invalidate on downgrade");
    CHECK(LogFind(OP_ERASE, 0) < 0, "no erase on downgrade");
    CHECK(LogFind(OP_WRITE, 0) < 0, "no write on downgrade");
    CHECK(LogFind(OP_SAVEMETA, 0) < 0, "no meta commit on downgrade");
    CHECK(s_meta.magic == META_MAGIC, "installed meta untouched");
    CHECK(s_watermark == 4096, "installed APP untouched");
    CHECK(flushrx_count >= 2, "receiver restarted");

    printf("test_downgrade_rejected: PASS\n");
}

/* 3. 传输截断 (协议完成但字节数不足): 不提交元数据 */
static void test_truncated_transfer_no_commit(void)
{
    uint8_t fw[300];

    SimReset(0);
    memset(fw, 0, sizeof(fw));

    Updater_Init();
    Updater_Begin(0);

    tx_header("fw.bin", sizeof(fw));
    tx_data(1, fw, 128);     /* 只发一包 (128/300) */
    tx_eot();
    tx_eot();
    tx_empty_header();
    PumpUpdater(3);        /* Finalize 检测到大小不符 -> Restart */

    CHECK(LogFind(OP_SAVEMETA, 0) < 0, "no meta commit on size mismatch");
    CHECK(s_meta.magic == 0, "meta stays invalid");
    CHECK(LogFind(OP_CALCCRC, 0) < 0, "no crc computed on mismatch");
    CHECK(reset_called == 0, "no reboot on mismatch");

    printf("test_truncated_transfer_no_commit: PASS\n");
}

/* 4. 元数据写入失败: 不复位, 回到等待, 重试可成功 */
static void test_meta_save_failure(void)
{
    static uint8_t fw[200];
    int i;

    SimReset(0);
    for (i = 0; i < (int)sizeof(fw); i++)
        fw[i] = (uint8_t)(i + 1);

    s_fail_save_meta = 1;

    Updater_Init();
    Updater_Begin(0);

    tx_header("fw_v1.9.bin", sizeof(fw));
    tx_firmware(fw, sizeof(fw));
    PumpUpdater(3);

    CHECK(LogFind(OP_SAVEMETA, 0) >= 0, "SaveMeta attempted");
    CHECK(s_meta.magic == 0, "meta not committed on failure");
    CHECK(reset_called == 0, "no reboot on meta failure");
    CHECK(flushrx_count >= 2, "receiver restarted for retry");

    /* 恢复后重新升级成功 (失败不锁死) */
    s_fail_save_meta = 0;
    SimReset(0);
    Updater_Init();
    Updater_Begin(0);
    tx_header("fw_v1.9.bin", sizeof(fw));
    tx_firmware(fw, sizeof(fw));
    PumpUpdater(3);
    CHECK(s_meta.magic == META_MAGIC, "retry after failure succeeds");

    printf("test_meta_save_failure: PASS\n");
}

/* 5. RX 溢出: 立即重启接收会话 */
static void test_rx_overflow_restart(void)
{
    SimReset(0);

    Updater_Init();
    Updater_Begin(0);
    flushrx_count   = 0;
    s_overflow_once = 1;

    Updater_Process();

    CHECK(flushrx_count == 1, "RX flushed on overflow");
    CHECK(ymodem_state() == YMODEM_WAIT_HEADER, "receiver re-armed");
    CHECK(LogFind(OP_ERASE, 0) < 0, "no erase on overflow");

    printf("test_rx_overflow_restart: PASS\n");
}

int main(void)
{
    test_full_update_sequence();
    test_downgrade_rejected();
    test_truncated_transfer_no_commit();
    test_meta_save_failure();
    test_rx_overflow_restart();

    if (test_failed)
    {
        printf("\n===== SOME TESTS FAILED =====\n");
        return 1;
    }
    printf("\n===== ALL TESTS PASSED =====\n");
    return 0;
}
