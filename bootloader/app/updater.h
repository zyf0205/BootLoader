#ifndef __UPDATER_H
#define __UPDATER_H

#include "stm32f4xx.h"

/* ================================================================== *
 * 固件升级状态机
 *
 *   WAITING --(收到文件头)--> RECEIVING --(YMODEM完成)--> SUCCESS -> 跳转APP
 *      ^                          |
 *      +--(传输中止, 自动重启)-----+
 *      | (窗口模式超时)
 *   TIMEOUT
 * ================================================================== */

typedef enum {
    UPDATER_WAITING = 0,    /* 等待 YMODEM 传输 ('C' 已发出) */
    UPDATER_RECEIVING,      /* 正在接收固件 */
    UPDATER_SUCCESS,        /* 升级完成, 等待跳转 */
    UPDATER_TIMEOUT,        /* 等待窗口超时 */
} updater_state_t;

void Updater_Init(void);

/* 开始等待: window_ms > 0 表示超时窗口 (Boot 阶段用), 0 表示无限等待 */
void Updater_Begin(uint32_t window_ms);

/* 主循环调用: 灌入串口数据 + 状态机推进 */
void Updater_Process(void);

updater_state_t Updater_State(void);

/* 流程是否已出结果 (SUCCESS 或 TIMEOUT) */
uint8_t Updater_Finished(void);

#endif /* __UPDATER_H */
