#ifndef __VERSION_H
#define __VERSION_H

/* ================================================================== *
 * Bootloader 版本信息
 * 协议: YMODEM
 * ================================================================== */

#define BL_VERSION_MAJOR    3
#define BL_VERSION_MINOR    3
#define BL_VERSION_PATCH    1

/* 字符串版本由数值宏派生, 避免两处手工同步 (曾因不一致单独修复过) */
#define BL_STR_(x)          #x
#define BL_STR(x)           BL_STR_(x)
#define BL_VERSION_STR      BL_STR(BL_VERSION_MAJOR) "." \
                            BL_STR(BL_VERSION_MINOR) "." \
                            BL_STR(BL_VERSION_PATCH)

#endif /* __VERSION_H */
