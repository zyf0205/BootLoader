#include "updater.h"
#include "ymodem.h"
#include "usart.h"
#include "flash.h"
#include "led.h"
#include "systick.h"
#include "boot.h"
#include "boot_config.h"
#include <stdio.h>

/* ---- 内部状态 ---- */
static updater_state_t s_state = UPDATER_WAITING;
static uint32_t s_window_start = 0;
static uint32_t s_window_ms    = 0;
static uint32_t s_done_tick    = 0;

/* ---- 传输上下文 ---- */
static uint32_t s_fw_size     = 0;
static uint32_t s_write_addr  = 0;
static uint16_t s_fw_version  = 0;

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
                major = major * 10 + (uint32_t)(*q - '0');
                q++;
            }

            if (q != p + 1 && *q == '.')
            {
                q++;
                while (*q >= '0' && *q <= '9')
                {
                    minor = minor * 10 + (uint32_t)(*q - '0');
                    q++;
                }
                if (major <= 0xFF && minor <= 0xFF)
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

/* 发送回调: 等待 TX 空闲后发送 (协议为同步请求-应答, 阻塞可接受) */
static int YmodemSend(const uint8_t *data, uint16_t len)
{
    while (Usart_Send(data, len) != 0)
    {
    }
    return 0;
}

/* ======================== YMODEM 回调 ======================== */

static int OnHeader(const char *name, uint32_t size)
{
    const char *base = BaseName(name);

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

    s_fw_size    = size;
    s_write_addr = APP_ADDR;
    s_fw_version = ParseVersion(base);

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

    /* YMODEM 末包按 1024/128 对齐填充, 实际写入量以文件大小为准 */
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

/* ======================== 升级流程 ======================== */

static void Restart(void)
{
    Led_Set(LED_BLINK_SLOW);
    printf("\r\nRestarting receiver, waiting for YMODEM...\r\n");
    ymodem_stop();
    Usart_FlushRx();   /* 丢弃残留脏数据, 避免污染新会话 (如 0x18 误判中止) */
    ymodem_start();
    s_state = UPDATER_WAITING;
}

/* 传输完成: CRC32 校验 + 保存元数据, 成功后延时跳转 APP */
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

    printf("Verifying CRC32...");
    crc = Flash_CalcAppCrc32(s_fw_size);
    printf("OK (0x%08lX)\r\n", (unsigned long)crc);

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

    printf("\r\nUpdate success! Jumping to APP in %d s...\r\n",
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

void Updater_Begin(uint32_t window_ms)
{
    s_window_ms    = window_ms;
    s_window_start = Systick_GetTick();
    s_state        = UPDATER_WAITING;

    Usart_FlushRx();   /* 丢弃上电/上次会话残留的 RX 脏数据 */
    ymodem_start();
}

void Updater_Process(void)
{
    /* 1. 灌入串口数据 */
    while (Usart_RxAvailable() > 0)
    {
        uint8_t byte;
        if (Usart_ReadByte(&byte) == 0)
            ymodem_feed(byte);
    }

    /* 2. 状态机推进 */
    switch (s_state)
    {
    case UPDATER_WAITING:
        if (ymodem_started())
        {
            s_state = UPDATER_RECEIVING;
        }
        else if (ymodem_state() == YMODEM_ABORTED)
        {
            Restart();   /* 文件头被拒 / 发送端中止 -> 重启接收 */
        }
        else if (s_window_ms != 0 &&
                 (Systick_GetTick() - s_window_start) >= s_window_ms)
        {
            ymodem_stop();
            s_state = UPDATER_TIMEOUT;
        }
        break;

    case UPDATER_RECEIVING:
        if (ymodem_state() == YMODEM_DONE)
            Finalize();
        else if (ymodem_state() == YMODEM_ABORTED)
            Restart();
        break;

    case UPDATER_SUCCESS:
        if ((Systick_GetTick() - s_done_tick) >= BOOT_JUMP_DELAY_MS)
        {
            Boot_JumpToApp();       /* 正常情况不会返回 */
            NVIC_SystemReset();     /* 跳转失败兜底: 复位重来 */
        }
        break;

    case UPDATER_TIMEOUT:
    default:
        break;
    }
}

updater_state_t Updater_State(void)
{
    return s_state;
}

uint8_t Updater_Finished(void)
{
    return (s_state == UPDATER_SUCCESS || s_state == UPDATER_TIMEOUT);
}
