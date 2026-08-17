#ifndef __YMODEM_H
#define __YMODEM_H

#include <stdint.h>

/* ================================================================== *
 * YMODEM 接收端状态机 (平台无关, 可移植到任意 MCU)
 *
 * 协议规格 (YMODEM, CRC 模式, 1024 字节数据包):
 *   启动:   接收端发送 'C' (0x43) 请求 CRC 模式传输
 *   文件头: 128 字节包 [SOH][00][FF][文件名\0大小\0...][CRC16]
 *   数据包: [STX|SOH][SEQ][~SEQ][DATA 1024|128][CRC16_H][CRC16_L]
 *   结束:   发送端发 EOT -> 接收端 NAK -> 发送端再发 EOT -> 接收端 ACK
 *           发送端发空文件头 -> 接收端 ACK -> 传输完成
 *   中止:   双方均可发送 CAN CAN
 *
 * 使用方式:
 *   1. ymodem_init() 注册回调
 *   2. ymodem_start() 开始等待文件
 *   3. 主循环: 每收到一个字节调用 ymodem_feed(), 每 1ms 调用 ymodem_tick()
 * ================================================================== */

typedef struct {
    /* 发送回调: 返回 0 成功, 非 0 失败 (实现为阻塞或非阻塞均可) */
    int  (*send)(const uint8_t *data, uint16_t len);

    /* 收到文件头: 返回 0 接受传输, 非 0 拒绝 (自动发送 CAN CAN 中止) */
    int  (*on_header)(const char *name, uint32_t size);

    /* 文件头 ACK 后、请求首个数据包前执行耗时准备操作 */
    int  (*on_prepare)(void);

    /* 收到数据包: 返回 0 接受, 非 0 拒绝 (自动中止传输) */
    int  (*on_data)(const uint8_t *data, uint16_t len);

    /* 传输完成 (收到结束空包) */
    void (*on_complete)(void);

    /* 传输中止, reason 为可读原因 */
    void (*on_abort)(const char *reason);
} ymodem_callbacks_t;

typedef enum {
    YMODEM_IDLE = 0,      /* 未开始 */
    YMODEM_WAIT_HEADER,   /* 等待文件头 ('C' 已发送) */
    YMODEM_RX_DATA,       /* 接收数据中 */
    YMODEM_WAIT_EOT,      /* 已收到首次 EOT, 等待二次 EOT 或结束空包 */
    YMODEM_DONE,          /* 传输完成 */
    YMODEM_ABORTED,       /* 已中止 */
} ymodem_state_t;

void ymodem_init(const ymodem_callbacks_t *cb);

/* 开始等待传输 (发送 'C') */
void ymodem_start(void);

/* 停止接收, 回到 IDLE (Boot 窗口超时使用) */
void ymodem_stop(void);

/* 喂入一个接收字节 */
void ymodem_feed(uint8_t byte);

/* 1ms 定时任务: 超时重发 */
void ymodem_tick(void);

/* 接收端主动中止 (发送 CAN CAN) */
void ymodem_abort(void);

ymodem_state_t ymodem_state(void);

/* 是否已收到有效文件头 (Boot 窗口用于判断用户是否开始升级) */
uint8_t ymodem_started(void);

#endif /* __YMODEM_H */
