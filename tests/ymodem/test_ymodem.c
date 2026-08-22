/* YMODEM 接收端单元测试: 模拟一个 YMODEM 发送端驱动完整传输流程 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ymodem.h"
#include "crc16.h"

/* ============ 测试基础设施 ============ */

/* 发送回调收集的字节队列 */
static uint8_t  tx_queue[512];
static int      tx_len = 0;

static int test_failed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        test_failed = 1; \
    } \
} while (0)

/* ============ 接收方回调 (模拟 updater.c 的行为) ============ */

static char     got_name[64];
static uint32_t got_size;
static uint8_t  got_data[256 * 128];    /* 容纳 seq 回绕用例 (256 包 x 128B) */
static uint32_t got_data_len;
static int      complete_called;
static char     abort_reason[64];
static int      abort_called;
static int      reject_header;    /* 1 = 拒绝文件头 */
static int      reject_data_at;   /* 在第 N 个数据包时拒绝 */

static int cb_send(const uint8_t *data, uint16_t len)
{
    memcpy(&tx_queue[tx_len], data, len);
    tx_len += len;
    return 0;
}

static int cb_on_header(const char *name, uint32_t size)
{
    if (reject_header)
        return -1;
    strncpy(got_name, name, sizeof(got_name) - 1);
    got_size = size;
    return 0;
}

static int cb_on_prepare(void)
{
    return 0;
}

static int data_pkt_cnt = 0;

/* 与真实 updater 一致: 只写入文件大小以内的数据 (忽略末包填充) */
static int cb_on_data(const uint8_t *data, uint16_t len)
{
    uint32_t remain;

    data_pkt_cnt++;
    if (reject_data_at != 0 && data_pkt_cnt == reject_data_at)
        return -1;

    remain = (got_size > got_data_len) ? (got_size - got_data_len) : 0;
    if (remain > 0)
    {
        uint16_t write_len = (remain < len) ? (uint16_t)remain : len;
        memcpy(&got_data[got_data_len], data, write_len);
        got_data_len += write_len;
    }
    return 0;
}

static void cb_on_complete(void)
{
    complete_called = 1;
}

static void cb_on_abort(const char *reason)
{
    abort_called = 1;
    strncpy(abort_reason, reason, sizeof(abort_reason) - 1);
}

static const ymodem_callbacks_t cbs = {
    .send        = cb_send,
    .on_header   = cb_on_header,
    .on_prepare  = cb_on_prepare,
    .on_data     = cb_on_data,
    .on_complete = cb_on_complete,
    .on_abort    = cb_on_abort,
};

/* ============ 发送端模拟 (标准 YMODEM: SOH/STX, 末包 0x1A 填充) ============ */

static void tx_raw_packet(uint8_t soh_stx, uint8_t seq,
                          const uint8_t *data, uint16_t len,
                          uint16_t corrupt_pos)
{
    uint16_t crc = Crc16_Calc(data, len);
    uint8_t  byte;

    byte = soh_stx;           ymodem_feed(byte);
    byte = seq;               ymodem_feed(byte);
    byte = (uint8_t)~seq;     ymodem_feed(byte);
    for (int i = 0; i < len; i++)
    {
        byte = data[i];
        if (corrupt_pos != 0xFFFF && i == corrupt_pos)
            byte ^= 0xFF;      /* 线路上破坏数据字节, CRC 仍按原数据计算 */
        ymodem_feed(byte);
    }
    byte = (uint8_t)(crc >> 8);   ymodem_feed(byte);
    byte = (uint8_t)(crc & 0xFF); ymodem_feed(byte);
}

static void tx_header(const char *name, uint32_t size)
{
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    snprintf((char *)data, 128, "%s%c%u", name, '\0', size);
    tx_raw_packet(0x01, 0x00, data, 128, 0xFFFF);
}

static void tx_data_packet(uint8_t seq, const uint8_t *data, uint16_t len)
{
    uint8_t buf[128];
    memset(buf, 0x1A, sizeof(buf));
    memcpy(buf, data, len);
    tx_raw_packet(0x01, seq, buf, 128, 0xFFFF);    /* SOH 128 字节包 */
}

static void tx_data_packet_1k(uint8_t seq, const uint8_t *data, uint16_t len)
{
    uint8_t buf[1024];
    memset(buf, 0x1A, sizeof(buf));
    memcpy(buf, data, len);
    tx_raw_packet(0x02, seq, buf, 1024, 0xFFFF);   /* STX 1K 数据包 */
}

static void tx_data_packet_corrupt(uint8_t seq, const uint8_t *data,
                                   uint16_t len, uint16_t pos)
{
    uint8_t buf[128];
    memset(buf, 0x1A, sizeof(buf));
    memcpy(buf, data, len);
    tx_raw_packet(0x01, seq, buf, 128, pos);
}

static void tx_eot(void) { ymodem_feed(0x04); }

static void tx_empty_header(void)
{
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    tx_raw_packet(0x01, 0x00, data, 128, 0xFFFF);
}

static void rx_reset(void) { tx_len = 0; }

static void advance_ms(int n)
{
    for (int i = 0; i < n; i++)
        ymodem_tick();
}

static uint8_t last_tx(void) { return tx_queue[tx_len - 1]; }

/* 标准结束序列: EOT -> NAK -> EOT -> ACK -> 空包 -> ACK */
static void tx_finish(void)
{
    tx_eot();
    tx_eot();
    tx_empty_header();
}

/* ============ 用例公共初始化 ============ */

static void reset_rx(void)
{
    memset(&got_data, 0, sizeof(got_data));
    got_data_len = 0;
    got_size = 0;
    got_name[0] = 0;
    complete_called = 0;
    abort_called = 0;
    abort_reason[0] = 0;
    reject_header = 0;
    reject_data_at = 0;
    data_pkt_cnt = 0;
    tx_len = 0;
}

/* 1. CRC16 检查值 */
static void test_crc16_check(void)
{
    uint16_t crc = Crc16_Calc((const uint8_t *)"123456789", 9);
    CHECK(crc == 0x31C3, "crc16 check value");
    printf("test_crc16_check: PASS (0x%04X)\n", crc);
}

/* 2. 完整成功传输 (多包 + 末包填充) */
static void test_full_transfer(void)
{
    uint8_t firmware[2500];
    uint32_t offset;
    uint8_t seq;
    uint16_t len;
    int i;

    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    for (i = 0; i < (int)sizeof(firmware); i++)
        firmware[i] = (uint8_t)(i * 7 + 3);

    CHECK(last_tx() == 0x43, "first response should be 'C'");

    tx_header("led_v1.2.bin", sizeof(firmware));
    CHECK(tx_len >= 2 && tx_queue[tx_len - 2] == 0x06,
          "ACK after header");
    CHECK(last_tx() == 0x43, "C before first data packet");
    CHECK(ymodem_state() == YMODEM_RX_DATA, "state RX_DATA after header");
    CHECK(strcmp(got_name, "led_v1.2.bin") == 0, "name parsed");
    CHECK(got_size == sizeof(firmware), "size parsed");
    CHECK(ymodem_started() == 1, "started flag");

    offset = 0;
    seq = 1;
    while (offset < sizeof(firmware))
    {
        len = (uint16_t)(sizeof(firmware) - offset);
        if (len > 128)
            len = 128;
        tx_data_packet(seq++, &firmware[offset], len);
        CHECK(last_tx() == 0x06, "ACK data packet");
        offset += len;
    }

    tx_eot();
    CHECK(ymodem_state() == YMODEM_WAIT_EOT, "WAIT_EOT after first EOT");
    CHECK(last_tx() == 0x15, "NAK after first EOT");

    tx_eot();
    CHECK(tx_len >= 2 && tx_queue[tx_len - 2] == 0x06,
          "ACK after second EOT");
    CHECK(last_tx() == 0x43, "C requests final empty header");

    tx_empty_header();
    CHECK(last_tx() == 0x06, "ACK after empty header");
    CHECK(ymodem_state() == YMODEM_DONE, "state DONE");
    CHECK(complete_called == 1, "complete callback");

    rx_reset();
    tx_empty_header();
    CHECK(tx_len == 0, "bytes ignored after DONE");
    CHECK(ymodem_state() == YMODEM_DONE, "stay DONE after stray bytes");
    CHECK(complete_called == 1, "complete callback not repeated");

    CHECK(got_data_len == sizeof(firmware), "data length");
    CHECK(memcmp(got_data, firmware, sizeof(firmware)) == 0, "data content");

    printf("test_full_transfer: PASS (%u bytes, %d pkts)\n", got_data_len, data_pkt_cnt);
}

/* 3. 短文件完整传输 */
static void test_128b_transfer(void)
{
    uint8_t firmware[300];
    int i;

    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    for (i = 0; i < (int)sizeof(firmware); i++)
        firmware[i] = (uint8_t)(i * 5 + 1);

    tx_header("app.bin", sizeof(firmware));
    tx_data_packet(1, &firmware[0], 128);
    CHECK(last_tx() == 0x06, "ACK 128B pkt1");
    tx_data_packet(2, &firmware[128], 128);
    CHECK(last_tx() == 0x06, "ACK 128B pkt2");
    tx_data_packet(3, &firmware[256],
                   (uint16_t)(sizeof(firmware) - 256));
    CHECK(last_tx() == 0x06, "ACK 128B pkt3");

    tx_finish();
    CHECK(ymodem_state() == YMODEM_DONE, "128B transfer DONE");
    CHECK(complete_called == 1, "128B complete callback");
    CHECK(got_data_len == sizeof(firmware), "128B data length");
    CHECK(memcmp(got_data, firmware, sizeof(firmware)) == 0,
          "128B data content");

    printf("test_128b_transfer: PASS (%u bytes, %d pkts)\n",
           got_data_len, data_pkt_cnt);
}

/* 4. 单次 EOT + 直接空包 (兼容变体) */
static void test_single_eot_variant(void)
{
    uint8_t firmware[200];
    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    for (int i = 0; i < 200; i++)
        firmware[i] = (uint8_t)i;

    tx_header("app.bin", 200);
    tx_data_packet(1, firmware, 128);
    tx_data_packet(2, firmware + 128, 72);
    tx_eot();
    CHECK(last_tx() == 0x15, "NAK after EOT");
    tx_empty_header();      /* 跳过二次 EOT, 直接空包 */
    CHECK(ymodem_state() == YMODEM_DONE, "DONE without second EOT");
    CHECK(complete_called == 1, "complete");
    CHECK(got_data_len == 200, "data len");
    CHECK(memcmp(got_data, firmware, 200) == 0, "data content");

    printf("test_single_eot_variant: PASS\n");
}

/* 4. CRC 错误 -> NAK -> 重传恢复 */
static void test_crc_error_recovery(void)
{
    uint8_t firmware[250];
    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    for (int i = 0; i < 250; i++)
        firmware[i] = (uint8_t)(i + 1);

    tx_header("app.bin", 250);
    CHECK(tx_len >= 2 && tx_queue[tx_len - 2] == 0x06,
          "ACK header");
    CHECK(last_tx() == 0x43, "C after header");

    tx_data_packet(1, firmware, 128);
    CHECK(last_tx() == 0x06, "ACK pkt1");

    /* 数据字节被破坏 -> NAK */
    tx_data_packet_corrupt(2, firmware + 128, 122, 50);
    CHECK(last_tx() == 0x15, "NAK bad pkt");
    CHECK(ymodem_state() == YMODEM_RX_DATA, "still RX_DATA after NAK");
    CHECK(got_data_len == 128, "bad pkt not stored");

    /* 重传正确包 */
    tx_data_packet(2, firmware + 128, 122);
    CHECK(last_tx() == 0x06, "ACK retransmit");

    tx_finish();
    CHECK(ymodem_state() == YMODEM_DONE, "DONE after recovery");
    CHECK(got_data_len == 250, "data len");
    CHECK(memcmp(got_data, firmware, 250) == 0, "data content");

    printf("test_crc_error_recovery: PASS\n");
}

/* 5. 重复包 (ACK 丢失模拟) */
static void test_duplicate_packet(void)
{
    uint8_t firmware[200];
    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    for (int i = 0; i < 200; i++)
        firmware[i] = (uint8_t)(i * 3);

    tx_header("app.bin", 200);
    tx_data_packet(1, firmware, 128);
    tx_data_packet(1, firmware, 128);   /* 重复 */
    CHECK(last_tx() == 0x06, "ACK duplicate");
    CHECK(got_data_len == 128, "duplicate not stored");

    tx_data_packet(2, firmware + 128, 72);
    tx_finish();
    CHECK(ymodem_state() == YMODEM_DONE, "DONE");
    CHECK(got_data_len == 200, "data len");
    CHECK(memcmp(got_data, firmware, 200) == 0, "data content");

    printf("test_duplicate_packet: PASS\n");
}

/* 6. 文件过大被拒绝 -> CAN CAN 中止 */
static void test_header_reject(void)
{
    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    reject_header = 1;
    tx_header("big.bin", 0x100000);   /* 1MB, 超过 240KB */

    CHECK(ymodem_state() == YMODEM_ABORTED, "ABORTED");
    CHECK(abort_called == 1, "abort callback");
    CHECK(last_tx() == 0x18 && tx_queue[tx_len - 2] == 0x18, "CAN CAN sent");

    printf("test_header_reject: PASS\n");
}

/* 7. 数据包被拒 -> 中止 */
static void test_data_reject(void)
{
    uint8_t firmware[200] = {0};
    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    reject_data_at = 2;
    tx_header("app.bin", 200);
    tx_data_packet(1, firmware, 128);
    CHECK(ymodem_state() == YMODEM_RX_DATA, "still RX after pkt1");
    tx_data_packet(2, firmware + 128, 72);
    CHECK(ymodem_state() == YMODEM_ABORTED, "ABORTED on reject");
    CHECK(last_tx() == 0x18 && tx_queue[tx_len - 2] == 0x18, "CAN CAN sent");

    printf("test_data_reject: PASS\n");
}

/* 8. 发送端 CAN 中止 */
static void test_sender_can(void)
{
    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    ymodem_feed(0x18);
    CHECK(ymodem_state() == YMODEM_ABORTED, "ABORTED on CAN");
    CHECK(abort_called == 1, "abort callback");

    printf("test_sender_can: PASS\n");
}

/* 9. 'C' 周期性重发 + stop */
static void test_c_resend(void)
{
    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    int first_len = tx_len;
    advance_ms(1000);
    CHECK(tx_len > first_len, "C resent after 1s");
    CHECK(last_tx() == 0x43, "resent byte is 'C'");
    CHECK(ymodem_state() == YMODEM_WAIT_HEADER, "still WAIT_HEADER");

    ymodem_stop();
    int len2 = tx_len;
    advance_ms(3000);
    CHECK(tx_len == len2, "no send after stop");
    CHECK(ymodem_state() == YMODEM_IDLE, "IDLE after stop");

    printf("test_c_resend: PASS\n");
}

/* 10. 数据等待超时 -> 重发 ACK -> 超限中止 */
static void test_data_timeout_abort(void)
{
    uint8_t firmware[100];
    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    tx_header("app.bin", 100);
    tx_data_packet(1, firmware, 100);   /* ACK 已发 */
    rx_reset();                          /* 清空记录 */

    /* 3s 无包 -> 重发 ACK; 连续超时 -> 中止 */
    advance_ms(3 * 1000);
    CHECK(tx_len > 0 && last_tx() == 0x06, "ACK resent on timeout");

    advance_ms(3 * 1000 * 10);
    CHECK(ymodem_state() == YMODEM_ABORTED, "ABORTED after retries");
    CHECK(abort_called == 1, "abort callback");
    CHECK(last_tx() == 0x18 && tx_queue[tx_len - 2] == 0x18, "CAN CAN sent");

    printf("test_data_timeout_abort: PASS\n");
}

/* 11. 标准 YMODEM 1K STX 数据包 */
static void test_1k_packet_transfer(void)
{
    uint8_t firmware[1552];
    int i;

    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    for (i = 0; i < (int)sizeof(firmware); i++)
        firmware[i] = (uint8_t)(i * 11 + 7);

    tx_header("App.bin", sizeof(firmware));
    tx_data_packet_1k(1, firmware, 1024);
    CHECK(last_tx() == 0x06, "ACK 1K packet");
    tx_data_packet_1k(2, firmware + 1024,
                      (uint16_t)(sizeof(firmware) - 1024));
    CHECK(last_tx() == 0x06, "ACK padded final 1K packet");

    tx_finish();
    CHECK(ymodem_state() == YMODEM_DONE, "1K transfer DONE");
    CHECK(complete_called == 1, "1K complete callback");
    CHECK(got_data_len == sizeof(firmware), "1K data length");
    CHECK(memcmp(got_data, firmware, sizeof(firmware)) == 0,
          "1K data content");

    printf("test_1k_packet_transfer: PASS (%u bytes)\n", got_data_len);
}

/* 12. seq 255->0 回绕: SOH 传输超过 255 包 (256*128 = 32768B) */
static void test_seq_wraparound(void)
{
    static uint8_t firmware[256 * 128];
    uint32_t offset;
    uint8_t seq;
    int i;

    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    for (i = 0; i < (int)sizeof(firmware); i++)
        firmware[i] = (uint8_t)(i * 13 + 5);

    tx_header("wrap.bin", sizeof(firmware));
    offset = 0;
    seq = 1;
    while (offset < sizeof(firmware))
    {
        tx_data_packet(seq++, &firmware[offset], 128);
        CHECK(last_tx() == 0x06, "ACK data packet (incl. wrapped seq 0)");
        offset += 128;
    }
    /* seq 依次为 1..255, 0 (第 256 包回绕), 1 */

    tx_finish();
    CHECK(ymodem_state() == YMODEM_DONE, "wraparound transfer DONE");
    CHECK(got_data_len == sizeof(firmware), "wraparound full length");
    CHECK(memcmp(got_data, firmware, sizeof(firmware)) == 0,
          "wraparound data content");

    printf("test_seq_wraparound: PASS (%u bytes, 256 pkts, seq wrapped)\n",
           got_data_len);
}

/* 13. 帧内超时: 发送端半包死机 -> 丢弃 -> 重传恢复 */
static void test_interbyte_timeout(void)
{
    uint8_t firmware[300];
    int i;

    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    for (i = 0; i < 300; i++)
        firmware[i] = (uint8_t)(i * 9 + 1);

    tx_header("app.bin", 300);
    tx_data_packet(1, firmware, 128);

    /* 发送端发出 SOH + 10 字节后死机静默 */
    ymodem_feed(0x01);
    for (i = 0; i < 10; i++)
        ymodem_feed((uint8_t)i);
    rx_reset();

    advance_ms(999);
    CHECK(tx_len == 0, "no response before interbyte timeout");
    advance_ms(2);
    CHECK(tx_len > 0 && last_tx() == 0x15, "NAK after interbyte timeout");
    CHECK(ymodem_state() == YMODEM_RX_DATA, "still RX_DATA, half packet discarded");
    CHECK(got_data_len == 128, "half packet not stored");

    /* 发送端恢复, 重传 seq2/seq3 */
    tx_data_packet(2, firmware + 128, 128);
    CHECK(last_tx() == 0x06, "ACK after recovery");
    tx_data_packet(3, firmware + 256, 44);
    tx_finish();
    CHECK(ymodem_state() == YMODEM_DONE, "DONE after interbyte recovery");
    CHECK(got_data_len == 300, "data len");
    CHECK(memcmp(got_data, firmware, 300) == 0, "data content");

    printf("test_interbyte_timeout: PASS\n");
}

/* 14. 包内 CAN CAN: 发送端在数据包中途取消, 快速识别中止 */
static void test_inpacket_can(void)
{
    uint8_t firmware[200] = {0};

    reset_rx();
    ymodem_init(&cbs);
    ymodem_start();

    tx_header("app.bin", 200);
    tx_data_packet(1, firmware, 128);

    /* 包中途收到 CAN CAN 后静默 */
    ymodem_feed(0x01);            /* SOH */
    ymodem_feed(0x02);            /* seq */
    ymodem_feed((uint8_t)~0x02);  /* ~seq */
    ymodem_feed(0x18);            /* CAN */
    ymodem_feed(0x18);            /* CAN */
    rx_reset();

    advance_ms(1001);
    CHECK(ymodem_state() == YMODEM_ABORTED, "ABORTED on in-packet CAN CAN");
    CHECK(abort_called == 1, "abort callback");
    CHECK(strcmp(abort_reason, "sender aborted") == 0, "abort reason");
    CHECK(last_tx() == 0x18 && tx_queue[tx_len - 2] == 0x18, "CAN CAN sent");

    printf("test_inpacket_can: PASS\n");
}

int main(void)
{
    test_crc16_check();
    test_full_transfer();
    test_128b_transfer();
    test_single_eot_variant();
    test_crc_error_recovery();
    test_duplicate_packet();
    test_header_reject();
    test_data_reject();
    test_sender_can();
    test_c_resend();
    test_data_timeout_abort();
    test_1k_packet_transfer();
    test_seq_wraparound();
    test_interbyte_timeout();
    test_inpacket_can();

    if (test_failed)
    {
        printf("\n===== SOME TESTS FAILED =====\n");
        return 1;
    }

    printf("\n===== ALL TESTS PASSED =====\n");
    return 0;
}
