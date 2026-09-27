#include "ymodem.h"
#include "crc16.h"
#include <stddef.h>

/* ---- 协议控制字符 ---- */
#define YM_SOH      0x01    /* 128 字节包起始 */
#define YM_STX      0x02    /* 1024 字节包起始 */
#define YM_EOT      0x04    /* 数据结束 */
#define YM_ACK      0x06
#define YM_NAK      0x15
#define YM_CAN      0x18    /* 中止 */
#define YM_C        0x43    /* CRC 模式请求 */

#define YM_PKT_128_SIZE   128
#define YM_PKT_1K_SIZE    1024
#define YM_MAX_PKT_SIZE   (YM_PKT_1K_SIZE + 5)   /* STX+SEQ+~SEQ+DATA+CRC2 */

/* ---- 时序参数 ---- */
#define YM_HEADER_RETRY_MS     1000   /* 'C' 重发间隔 (无上限, 由上层决定何时放弃) */
#define YM_PKT_TIMEOUT_MS      3000   /* 等待数据包超时 */
#define YM_INTERBYTE_TIMEOUT_MS 1000  /* 帧内超时: 半包滞留判定 */
#define YM_MAX_ERRORS          5      /* 连续错误/超时次数上限 */

/* ======================== 内部状态 ======================== */

static const ymodem_callbacks_t *s_cb = NULL;

static ymodem_state_t s_state = YMODEM_IDLE;
static uint8_t  s_started = 0;          /* 已收到有效文件头 */
static uint8_t  s_wait_end_hdr = 0;     /* WAIT_EOT: 已 ACK 二次 EOT, 等待空包 */

static uint8_t  s_pkt[YM_MAX_PKT_SIZE];
static uint16_t s_pkt_idx = 0;          /* 已收字节数 */
static uint16_t s_pkt_len = 0;          /* 期望包总长 */
static uint16_t s_pkt_data_size = 0;    /* 数据段长度 128/1024 */

static uint8_t  s_expected_seq = 1;     /* 下一个期望的数据包序号 */
static uint8_t  s_error_cnt = 0;        /* 连续错误计数, 达上限中止会话 */
static uint32_t s_tick_ms = 0;          /* 协议内部毫秒时钟 (ymodem_tick 累加) */
static uint32_t s_last_resp_tick = 0;   /* 上次发出响应的时刻 (空闲重发计时) */
static uint32_t s_last_byte_tick = 0;   /* 最近一次收到字节的时刻 (帧内超时用) */
static uint8_t  s_last_resp = YM_C;     /* 最后发出的响应字节 (等包超时时原样重发) */

/* ======================== 发送辅助 ======================== */

static void SendByte(uint8_t b)
{
    if (s_cb->send)
        s_cb->send(&b, 1);
}

static void SendAck(void)
{
    s_last_resp = YM_ACK;
    s_last_resp_tick = s_tick_ms;
    SendByte(YM_ACK);
}

static void SendNak(void)
{
    s_last_resp = YM_NAK;
    s_last_resp_tick = s_tick_ms;
    SendByte(YM_NAK);
}

static void SendC(void)
{
    s_last_resp = YM_C;
    s_last_resp_tick = s_tick_ms;
    SendByte(YM_C);
}

static void SendAbort(void)
{
    SendByte(YM_CAN);
    SendByte(YM_CAN);
}

static void DoAbort(const char *reason)
{
    SendAbort();
    s_state = YMODEM_ABORTED;
    if (s_cb->on_abort)
        s_cb->on_abort(reason);
}

/* ======================== 数据解析 ======================== */

static void OnPacketError(void)
{
    /* 包已校验失败时, 检查原始字节流是否含 CAN CAN:
     * 发送端在包中途按了取消, 剩余字节不足以组成完整包,
     * 与其重传 5 次超时, 不如立即识别中止。合法数据包中的
     * 0x18 0x18 字节因 CRC 校验通过不会走到这里。 */
    uint16_t i;
    uint16_t len = (s_pkt_idx != 0) ? s_pkt_idx : s_pkt_len;

    for (i = 1; i < len; i++)
    {
        if (s_pkt[i - 1] == YM_CAN && s_pkt[i] == YM_CAN)
        {
            DoAbort("sender aborted");
            return;
        }
    }

    if (++s_error_cnt >= YM_MAX_ERRORS)
    {
        DoAbort("too many packet errors");
        return;
    }
    SendNak();   /* 请求发送端重传 */
}

/* 解析文件头包: 文件名\0大小\0... */
static void HandleHeader(const uint8_t *data)
{
    char     name[64];
    uint32_t size = 0;
    uint16_t i, n, digits;

    for (i = 0, n = 0; i < YM_PKT_128_SIZE && data[i] != '\0' && n < sizeof(name) - 1; i++)
        name[n++] = (char)data[i];
    name[n] = '\0';

    if (n >= sizeof(name) - 1 && data[n] != '\0')
    {
        /* 文件名占满缓冲仍未终止: 非法文件头。截断处理会把
         * 文件名剩余字符误当作大小字段解析, 必须整包拒绝 */
        OnPacketError();
        return;
    }
    i++;    /* 跳过 '\0' */

    /* 大小: 十进制数字串, 以空格或 '\0' 结束。
     * 最多取 9 位有效数字, 防止超长数字串溢出回绕绕过大小检查 */
    for (digits = 0; i < YM_PKT_128_SIZE && data[i] >= '0' && data[i] <= '9'; i++)
    {
        if (digits < 9)
            size = size * 10 + (uint32_t)(data[i] - '0');
        digits++;
    }

    if (name[0] == '\0' || size == 0)
    {
        OnPacketError();
        return;
    }

    /* 上层拒绝 (如固件过大) -> 中止整个会话 */
    if (s_cb->on_header && s_cb->on_header(name, size) != 0)
    {
        DoAbort("header rejected");
        return;
    }

    s_started        = 1;
    s_expected_seq   = 1;
    s_error_cnt      = 0;
    s_wait_end_hdr   = 0;
    s_state          = YMODEM_RX_DATA;
    SendAck();

    /* Standard YMODEM waits for a second 'C' after the header ACK. This
     * gives the receiver time to erase Flash before packet 1 is sent. */
    if (s_cb->on_prepare && s_cb->on_prepare() != 0)
    {
        DoAbort("receiver prepare failed");
        return;
    }
    SendC();
}

static void HandleData(const uint8_t *data)
{
    uint8_t seq = s_pkt[1];

    if (seq == s_expected_seq)
    {
        /* 新包: 交给上层处理 */
        if (s_cb->on_data && s_cb->on_data(data, s_pkt_data_size) != 0)
        {
            DoAbort("receiver rejected data");
            return;
        }
        s_expected_seq++;
        SendAck();
    }
    else if ((uint8_t)(seq + 1) == s_expected_seq)
    {
        /* 重复包: 上次 ACK 丢失, 重发 ACK */
        SendAck();
    }
    else
    {
        /* 序号错乱 */
        OnPacketError();
    }
}

static void HandlePacket(void)
{
    uint8_t  seq  = s_pkt[1];
    uint8_t  seqc = s_pkt[2];
    const uint8_t *data = &s_pkt[3];
    uint16_t recv_crc = (uint16_t)((s_pkt[s_pkt_len - 2] << 8) | s_pkt[s_pkt_len - 1]);
    uint16_t calc_crc = Crc16_Calc(data, s_pkt_data_size);

    /* 帧完整性校验 */
    if ((uint8_t)(seq + seqc) != 0xFF || recv_crc != calc_crc)
    {
        OnPacketError();
        return;
    }

    s_error_cnt = 0;

    if (seq == 0 && s_state != YMODEM_RX_DATA)
    {
        /* seq 0: 文件头包 / 结束空包。RX_DATA 态下的 seq 0 是
         * 序号 255->0 自然回绕后的第 256 个数据包, 走下方数据包路径 */
        if (s_state == YMODEM_WAIT_HEADER)
        {
            HandleHeader(data);
        }
        else if ((s_state == YMODEM_WAIT_EOT || s_state == YMODEM_DONE) &&
                 data[0] == '\0')
        {
            /* 最终 ACK 丢失时发送端会重发结束空包, 继续 ACK。 */
            SendAck();
            if (s_state != YMODEM_DONE)
            {
                s_state = YMODEM_DONE;
                if (s_cb->on_complete)
                    s_cb->on_complete();
            }
        }
        else
        {
            OnPacketError();
        }
    }
    else
    {
        /* 数据包 */
        if (s_state == YMODEM_RX_DATA)
            HandleData(data);
        else
            OnPacketError();
    }
}

static void HandleEot(void)
{
    if (s_state == YMODEM_RX_DATA)
    {
        /* 首次 EOT: NAK, 等待发送端重发 */
        SendNak();
        s_state        = YMODEM_WAIT_EOT;
        s_wait_end_hdr = 0;
    }
    else if (s_state == YMODEM_WAIT_EOT && !s_wait_end_hdr)
    {
        /* 二次 EOT: ACK + 'C', 请求发送端发结束空包 */
        SendAck();
        s_wait_end_hdr = 1;
        SendC();
    }
    else
    {
        OnPacketError();
    }
}

/* ======================== 公开接口 ======================== */

void ymodem_init(const ymodem_callbacks_t *cb)
{
    s_cb = cb;
    ymodem_stop();
}

void ymodem_start(void)
{
    s_state        = YMODEM_WAIT_HEADER;
    s_started      = 0;
    s_pkt_idx      = 0;
    s_expected_seq = 1;
    s_error_cnt    = 0;
    s_wait_end_hdr = 0;
    SendC();
}

void ymodem_stop(void)
{
    s_state        = YMODEM_IDLE;
    s_started      = 0;
    s_pkt_idx      = 0;
    s_expected_seq = 1;
    s_error_cnt    = 0;
    s_wait_end_hdr = 0;
}

void ymodem_feed(uint8_t byte)
{
    /* DONE 后会话已结束, 后续杂散字节全部忽略, 避免组包产生
     * 多余 NAK 污染链路 (成功路径由上层直接接管复位) */
    if (s_state == YMODEM_IDLE || s_state == YMODEM_ABORTED ||
        s_state == YMODEM_DONE)
        return;

    s_last_byte_tick = s_tick_ms;

    if (s_pkt_idx == 0)
    {
        /* CAN 仅在包间隙有效 (包内字节可能等于 0x18) */
        if (byte == YM_CAN)
        {
            SendAbort();
            s_state = YMODEM_ABORTED;
            if (s_cb->on_abort)
                s_cb->on_abort("sender aborted");
            return;
        }

        /* 等待包起始字节 */
        if (byte == YM_SOH)
        {
            s_pkt[0] = byte;
            s_pkt_data_size = YM_PKT_128_SIZE;
            s_pkt_len = YM_PKT_128_SIZE + 5;
            s_pkt_idx = 1;
        }
        else if (byte == YM_STX)
        {
            s_pkt[0] = byte;
            s_pkt_data_size = YM_PKT_1K_SIZE;
            s_pkt_len = YM_PKT_1K_SIZE + 5;
            s_pkt_idx = 1;
        }
        else if (byte == YM_EOT && s_state != YMODEM_WAIT_HEADER)
        {
            HandleEot();
        }
        /* 其余杂散字节忽略 */
    }
    else
    {
        s_pkt[s_pkt_idx++] = byte;
        if (s_pkt_idx >= s_pkt_len)
        {
            HandlePacket();
            s_pkt_idx = 0;
        }
    }
}

void ymodem_tick(void)
{
    uint32_t elapsed;

    s_tick_ms++;

    if (s_state == YMODEM_IDLE ||
        s_state == YMODEM_DONE ||
        s_state == YMODEM_ABORTED)
        return;

    /* 帧内超时: 发送端发出包起始后死机/拔线, 半包永久滞留。
     * 丢弃半包并按错误处理 (要求重传), 不再永久阻塞 */
    if (s_pkt_idx != 0)
    {
        if ((s_tick_ms - s_last_byte_tick) >= YM_INTERBYTE_TIMEOUT_MS)
        {
            s_pkt_idx = 0;
            OnPacketError();
        }
        return;
    }

    elapsed = s_tick_ms - s_last_resp_tick;

    if (s_state == YMODEM_WAIT_HEADER)
    {
        /* 周期性重发 'C', 直到上层放弃等待 */
        if (elapsed >= YM_HEADER_RETRY_MS)
            SendC();
    }
    else if (elapsed >= YM_PKT_TIMEOUT_MS)
    {
        /* 等待数据包超时: 重发上次响应, 超限中止 */
        if (++s_error_cnt >= YM_MAX_ERRORS)
        {
            DoAbort("timeout");
            return;
        }

        s_last_resp_tick = s_tick_ms;
        SendByte(s_last_resp);
    }
}

void ymodem_abort(void)
{
    DoAbort("receiver aborted");
}

ymodem_state_t ymodem_state(void)
{
    return s_state;
}

uint8_t ymodem_started(void)
{
    return s_started;
}
