#ifndef __CRC16_H
#define __CRC16_H

#include <stdint.h>

/* CRC16-CCITT (XMODEM 变体)
 *   多项式: 0x1021
 *   初始值: 0x0000 (与 XMODEM 标准一致, 区别于普通 CCITT 的 0xFFFF)
 *   无输入/输出反转
 *   校验值: "123456789" -> 0x31C3
 */
uint16_t Crc16_Calc(const uint8_t *data, uint32_t len);

#endif /* __CRC16_H */
