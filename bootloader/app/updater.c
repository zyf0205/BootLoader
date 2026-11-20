#include "updater.h"
#include "ymodem.h"
#include "usart.h"
#include "flash.h"
#include "led.h"
#include "systick.h"
#include "boot.h"
#include "boot_policy.h"
#include "boot_config.h"
#include <stdio.h>

/* ---- 内部状态 ---- */
static updater_state_t s_state = UPDATER_WAITING;
static uint32_t s_done_tick = 0;    /* 升级完成时刻 (SUCCESS 延时跳转用) */

/* ---- 传输上下文 ---- */
static uint32_t s_fw_size     = 0;     /* 本次固件总大小 (字节) */
static uint32_t s_write_addr  = 0;     /* 下一包的写入地址 (APP 区内递增) */
static uint16_t s_fw_version  = 0;     /* 文件名解析出的版本, 0 = 未知 */

/* ======================== 辅助函数 ======================== */

/* 取文件名 (去掉路径前缀) */
static const char *BaseName(const char *name)
{
    const char *p = name;
    const char *base = name;

    while (*p)
    {
        if (*p == '/' || *p == '\\')
            base = p + 1;
        p++;
    }
    return base;
}

/* 从文件名解析 "vX.Y" 版本, 如 "led_v1.2.bin" -> 0x0102, 失败返回 0 */
static uint16_t ParseVersion(const char *name)
{
    const char *p = name;

    while (*p)
    {
        if (*p == 'v' || *p == 'V')
        {
            uint32_t major = 0, minor = 0;
            const char *q = p + 1;

            while (*q >= '0' && *q <= '9')
            {
                /* 超过 0xFF 后停止累加, 防止超长数字串溢出回绕成合法版本 */
                if (major <= 0xFFu)
                    major = major * 10 + (uint32_t)(*q - '0');
                q++;
            }

            if (q != p + 1 && *q == '.')
            {
                q++;
                while (*q >= '0' && *q <= '9')
                {
                    if (minor <= 0xFFu)
                        minor = minor * 10 + (uint32_t)(*q - '0');
                    q++;
                }
                if (major <= 0xFFu && minor <= 0xFFu)
                    return (uint16_t)((major << 8) | minor);
            }
        }
        p++;
    }
    return 0;
}

static uint8_t IsHexFile(const char *name)
{
    const char *p = name;
    const char *ext = NULL;

    while (*p)
    {
        if (*p == '.')
            ext = p + 1;
        p++;
    }

    return ext != NULL &&
           (ext[0] == 'h' || ext[0] == 'H') &&
           (ext[1] == 'e' || ext[1] == 'E') &&
           (ext[2] == 'x' || ext[2] == 'X') &&
           ext[3] == '\0';
}

/* 协议控制字节使用阻塞发送，确保 ACK/NAK/C 已交给 USART 后再推进状态机。 */
static int YmodemSend(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
        Usart_Putc(data[i]);
    return 0;
}

/* ======================== YMODEM 回调 ======================== */

static int OnHeader(const char *name, uint32_t size)
{
    const char *base = BaseName(name);
    uint8_t    app_valid;
    uint16_t   incoming_ver;
    fw_meta_t  meta;

    if (IsHexFile(base))
    {
        printf("\r\nError: HEX files are not supported. Send App.bin.\r\n");
        return -1;
    }

    /* 大小检查 */
    if (size == 0 || size > APP_SIZE)
    {
        printf("\r\nError: file size %lu out of range (max %lu).\r\n",
               (unsigned long)size, (unsigned long)APP_SIZE);
        return -1;
    }

    /* 防降级: 仅当当前 APP 实际完好 (元数据 + 向量表 + CRC32 全部
     * 通过) 时拒绝更旧版本。元数据有效但固件已损坏 (如 SWD 直刷裸
     * 固件) 时放行任意版本, 保证旧版本可救砖 */
    app_valid    = Flash_IsAppValid();
    incoming_ver = ParseVersion(base);
    if (app_valid && Flash_LoadMeta(&meta) &&
        !BootPolicy_AllowUpgrade(app_valid, meta.version, incoming_ver))
    {
        printf("\r\nError: downgrade rejected (installed v%u.%u, incoming v%u.%u).\r\n",
               meta.version >> 8, meta.version & 0xFF,
               incoming_ver >> 8, incoming_ver & 0xFF);
        return -1;
    }

    s_fw_size    = size;
    s_write_addr = APP_ADDR;
    s_fw_version = incoming_ver;

    printf("\r\nFile   : %s\r\n", base);
    printf("Size   : %lu bytes\r\n", (unsigned long)size);
    if (s_fw_version != 0)
        printf("Version: v%d.%d\r\n", s_fw_version >> 8, s_fw_version & 0xFF);

    Led_Set(LED_BLINK_FAST);
    return 0;
}

static int OnPrepare(void)
{
    flash_err_t err;

    printf("Invalidating old metadata...");
    err = Flash_InvalidateMeta();
    if (err != FLASH_OK)
    {
        printf("FAILED\r\n");
        return -1;
    }
    printf("OK\r\n");

    printf("Erasing APP region...");
    err = Flash_EraseApp();
    if (err != FLASH_OK)
    {
        printf("FAILED\r\n");
        return -1;
    }
    printf("OK\r\n\r\nReceiving ");
    return 0;
}

static int OnData(const uint8_t *data, uint16_t len)
{
    flash_err_t err;
    uint32_t written = s_write_addr - APP_ADDR;
    uint32_t remain  = (s_fw_size > written) ? (s_fw_size - written) : 0;

    /* YMODEM 末包按 128/1024 字节对齐填充, 实际写入量以文件大小为准 */
    uint16_t write_len = (remain < len) ? (uint16_t)remain : len;
    if (write_len == 0)
    {
        /* 数据已收满, 忽略多余的填充包 */
        return 0;
    }

    if (s_write_addr + write_len > APP_END_ADDR)
    {
        printf("\r\nError: address overflow.\r\n");
        return -1;
    }

    err = Flash_Write(s_write_addr, data, write_len);
    if (err != FLASH_OK)
    {
        printf("\r\nError: flash write failed (%d).\r\n", (int)err);
        return -1;
    }

    s_write_addr += write_len;
    return 0;
}

static void OnAbort(const char *reason)
{
    printf("\r\nAborted: %s\r\n", reason);
}

/* 会话是否已越过"等待文件头"阶段: 只有受理了文件头才会进入这三个状态 */
static uint8_t SessionStarted(void)
{
    ymodem_state_t st = ymodem_state();

    return (st == YMODEM_RX_DATA || st == YMODEM_WAIT_EOT || st == YMODEM_DONE);
}

/* ======================== 升级流程 ======================== */

static void Restart(void)
{
    Led_Set(LED_BLINK_SLOW);   /* 回到等待状态, 恢复慢闪指示 */
    printf("\r\nRestarting receiver, waiting for YMODEM...\r\n");
    ymodem_stop();
    Usart_FlushRx();   /* 丢弃残留脏数据, 避免污染新会话 (如 0x18 误判中止) */
    ymodem_start();
    s_state = UPDATER_WAITING;
}

/* 传输完成: CRC32 校验 + 保存元数据, 成功后延时复位启动 APP */
static void Finalize(void)
{
    fw_meta_t meta;
    uint32_t crc;

    printf("done.\r\n");

    if ((s_write_addr - APP_ADDR) != s_fw_size)
    {
        printf("Error: size mismatch (%lu != %lu).\r\n",
               (unsigned long)(s_write_addr - APP_ADDR),
               (unsigned long)s_fw_size);
        Restart();
        return;
    }

    printf("Calculating CRC32...");
    crc = Flash_CalcAppCrc32(s_fw_size);
    printf("done (0x%08lX)\r\n", (unsigned long)crc);

    printf("Saving metadata...");
    meta.magic    = META_MAGIC;
    meta.size     = s_fw_size;
    meta.crc32    = crc;
    meta.version  = s_fw_version;
    meta.reserved = 0;

    if (Flash_SaveMeta(&meta) != FLASH_OK)
    {
        printf("FAILED\r\n");
        Restart();
        return;
    }
    printf("OK\r\n");

    /* 升级成功: 固件已被校验与提交, Fault 计数失去意义, 复位前清零。
     * 未在此路径清零的 Fault 计数 (recovery 中放弃升级直接断电) 会
     * 继续闩锁, 直到新 APP 稳定运行后调用 Boot_ReportHealthy() */
    Boot_ClearFaultResets();

    printf("\r\nUpdate success! Rebooting to APP in %d s...\r\n",
           BOOT_JUMP_DELAY_MS / 1000);
    Led_Set(LED_ON);

    s_done_tick = Systick_GetTick();
    s_state     = UPDATER_SUCCESS;
}

/* ======================== 公开接口 ======================== */

void Updater_Init(void)
{
    static const ymodem_callbacks_t callbacks = {
        .send        = YmodemSend,
        .on_header   = OnHeader,
        .on_prepare  = OnPrepare,
        .on_data     = OnData,
        .on_complete = NULL,    /* 完成逻辑在 Updater_Process 中处理 */
        .on_abort    = OnAbort,
    };

    ymodem_init(&callbacks);
    s_state = UPDATER_WAITING;
}

void Updater_Begin(void)
{
    s_state = UPDATER_WAITING;

    Usart_FlushRx();   /* 丢弃上电/上次会话残留的 RX 脏数据 */
    ymodem_start();
}

void Updater_Process(void)
{
    uint8_t byte;

    /* 0. RX 缓冲溢出检测 (仅接收相关状态): DMA 已绕过未读数据
     *    (如发送端不等 'C' 在擦除期间硬冲), 缓冲内容不可信, 重启
     *    接收会话。SUCCESS 阶段的残留垃圾不重新开放会话,
     *    避免刚提交的固件被新会话擦除 */
    if ((s_state == UPDATER_WAITING || s_state == UPDATER_RECEIVING) &&
        Usart_TakeRxOverflow())
    {
        printf("\r\nRX buffer overflow, restarting receiver...\r\n");
        Restart();
        return;
    }

    /* 1. 灌入串口数据 (ReadByte 内部已含可用字节更新与溢出检测) */
    while (Usart_ReadByte(&byte) == 0)
        ymodem_feed(byte);

    /* 2. 状态机推进 */
    switch (s_state)
    {
    case UPDATER_WAITING:
        if (ymodem_state() == YMODEM_ABORTED)
            Restart();   /* 文件头被拒 / 发送端中止 -> 重启接收 */
        else if (SessionStarted())
            s_state = UPDATER_RECEIVING;   /* 文件头已受理 */
        break;

    case UPDATER_RECEIVING:
        if (ymodem_state() == YMODEM_DONE)
            Finalize();
        else if (ymodem_state() == YMODEM_ABORTED)
            Restart();
        break;

    case UPDATER_SUCCESS:
        if ((Systick_GetTick() - s_done_tick) >= BOOT_JUMP_DELAY_MS)
            Boot_ResetAfterUpdate();
        break;

    default:
        break;
    }
}