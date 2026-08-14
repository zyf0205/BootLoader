#ifndef __BOARD_H
#define __BOARD_H

/* ================================================================== *
 * 开发板资源定义 - STM32F411CEU6 (Black Pill)
 *
 * 本头文件是模块层与具体硬件的唯一耦合点:
 * 更换板卡时只需修改本文件, 所有驱动无需改动。
 * ================================================================== */

/* ---- 状态 LED ---- */
#define LED_PORT        GPIOA
#define LED_PIN         GPIO_Pin_1
#define LED_CLK         RCC_AHB1Periph_GPIOA
#define LED_ACTIVE_LOW  1               /* 1: 低电平点亮 */

/* ---- USER KEY ---- */
#define KEY_PORT        GPIOC
#define KEY_PIN         GPIO_Pin_13
#define KEY_CLK         RCC_AHB1Periph_GPIOC
#define KEY_ACTIVE_LOW  1               /* 1: 低电平按下 */

/* ---- 调试串口 (USART1: PA9-TX, PA10-RX) ---- */
#define CONSOLE_USART   USART1
#define CONSOLE_GPIO    GPIOA
#define CONSOLE_TX_PIN  GPIO_Pin_9
#define CONSOLE_RX_PIN  GPIO_Pin_10
#define CONSOLE_GPIO_CLK RCC_AHB1Periph_GPIOA
#define CONSOLE_USART_CLK RCC_APB2Periph_USART1
#define CONSOLE_BAUDRATE 115200

#endif /* __BOARD_H */
