#include "key.h"
#include "board.h"

#define KEY_DEBOUNCE_MS  10    /* 消抖时间 */

/* ---- 内部状态 ---- */
static uint8_t  s_stable_level;    /* 消抖后的稳定电平 */
static uint8_t  s_last_level;      /* 上一次采样电平 */
static uint16_t s_debounce_cnt;    /* 电平保持计数 */
static uint8_t  s_event;           /* 按下沿事件标志 */

static uint8_t Key_ReadRaw(void)
{
#if KEY_ACTIVE_LOW
    return (GPIO_ReadInputDataBit(KEY_PORT, KEY_PIN) == Bit_RESET);
#else
    return (GPIO_ReadInputDataBit(KEY_PORT, KEY_PIN) != Bit_RESET);
#endif
}

void Key_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(KEY_CLK, ENABLE);

    gpio.GPIO_Pin   = KEY_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IN;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(KEY_PORT, &gpio);

    s_stable_level = Key_ReadRaw();
    s_last_level   = s_stable_level;
    s_debounce_cnt = 0;
    s_event        = 0;
}

uint8_t Key_Read(void)
{
    return s_stable_level;
}

uint8_t Key_GetEvent(void)
{
    uint8_t event = s_event;
    s_event = 0;
    return event;
}

/* 状态机消抖: 电平连续保持 N ms 才认为有效 */
void Key_Tick(void)
{
    uint8_t raw = Key_ReadRaw();

    if (raw == s_last_level)
    {
        if (s_debounce_cnt < KEY_DEBOUNCE_MS)
        {
            s_debounce_cnt++;
            if (s_debounce_cnt == KEY_DEBOUNCE_MS)
            {
                /* 电平稳定, 确认 */
                if (raw && !s_stable_level)
                {
                    s_event = 1;    /* 按下沿 */
                }
                s_stable_level = raw;
            }
        }
    }
    else
    {
        s_last_level   = raw;
        s_debounce_cnt = 0;
    }
}
