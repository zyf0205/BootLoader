#ifndef __UPDATER_H
#define __UPDATER_H

#include "stm32f4xx.h"

/* ================================================================== *
 * 固件升级状态机
 *
 *   WAITING --(收到文件头)--> RECEIVING --(YMODEM完成)--> SUCCESS -> 延时复位
 *      ^                          |
 *      +--(传输中止, 自动重启)-----+
 * ================================================================== */

typedef enum {
    UPDATER_WAITING = 0,    /* 等待 YMODEM 传输 ('C' 已发出) */
    UPDATER_RECEIVING,      /* 正在接收固件 */
    UPDATER_SUCCESS,        /* 升级完成, 等待跳转 */
} updater_state_t;

void Updater_Init(void);

/* 开始等待: 无限等待, 直到收到文件头或发送端中止 */
void Updater_Begin(void);

/* 主循环调用: 灌入串口数据 + 状态机推进 */
void Updater_Process(void);

#endif /* __UPDATER_H */
