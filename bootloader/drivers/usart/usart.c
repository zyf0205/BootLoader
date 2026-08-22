#include "usart.h"
#include "board.h"
#include "flash_config.h"
#include "systick.h"
#include <string.h>
#include <stdio.h>

/* ================================================================== *
 * DMA 映射 (USART1 固定使用, 更换串口时需同步修改)
 *   RX: DMA2_Stream2 / Channel 4   (Circular)
 *   TX: DMA2_Stream7 / Channel 4   (Normal + TC 中断)
 * ================================================================== */
#define RX_DMA_STREAM   DMA2_Stream2
#define RX_DMA_CHANNEL  DMA_Channel_4
#define TX_DMA_STREAM   DMA2_Stream7
#define TX_DMA_CHANNEL  DMA_Channel_4
#define TX_DRAIN_TIMEOUT_MS  100

/* ---- RX 环形缓冲 ---- */
static uint8_t  s_rx_buf[USART_RX_BUF_SIZE];
static uint16_t s_rx_read_pos = 0;      /* 消费位置 (仅主循环访问) */
static uint16_t s_rx_last_write = 0;    /* 上次轮询时的写位置 (溢出检测) */
static uint8_t  s_rx_overflow = 0;      /* DMA 覆盖未读数据标志 */

/* ---- TX DMA 缓冲 ---- */
static uint8_t s_tx_buf[USART_TX_BUF_SIZE];
static volatile uint8_t s_tx_active = 0;

/* ---- printf 重定向 (microlib) ---- */
struct __FILE { int handle; };
FILE __stdout;

int fputc(int ch, FILE *f)
{
    (void)f;
    Usart_Putc((uint8_t)ch);
    return ch;
}

/* ======================== 内部实现 ======================== */

/* DMA 当前写入位置 (环形缓冲写指针) */
static uint16_t Usart_RxWritePos(void)
{
    return USART_RX_BUF_SIZE - (uint16_t)DMA_GetCurrDataCounter(RX_DMA_STREAM);
}

void Usart_Init(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef  nvic;
    DMA_InitTypeDef   dma;

    /* 1. 时钟 */
    RCC_AHB1PeriphClockCmd(CONSOLE_GPIO_CLK | RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(CONSOLE_USART_CLK, ENABLE);

    /* 2. GPIO: PA9=TX, PA10=RX (USART1 专用) */
    gpio.GPIO_Pin   = CONSOLE_TX_PIN | CONSOLE_RX_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(CONSOLE_GPIO, &gpio);

    GPIO_PinAFConfig(CONSOLE_GPIO, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(CONSOLE_GPIO, GPIO_PinSource10, GPIO_AF_USART1);

    /* 3. USART 参数 */
    usart.USART_BaudRate   = CONSOLE_BAUDRATE;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits   = USART_StopBits_1;
    usart.USART_Parity     = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode       = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(CONSOLE_USART, &usart);

    /* 4. RX DMA: 环形模式, 持续接收 */
    DMA_DeInit(RX_DMA_STREAM);
    dma.DMA_Channel            = RX_DMA_CHANNEL;
    dma.DMA_PeripheralBaseAddr = (uint32_t)&CONSOLE_USART->DR;
    dma.DMA_Memory0BaseAddr    = (uint32_t)s_rx_buf;
    dma.DMA_DIR                = DMA_DIR_PeripheralToMemory;
    dma.DMA_BufferSize         = USART_RX_BUF_SIZE;
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode               = DMA_Mode_Circular;
    dma.DMA_Priority           = DMA_Priority_High;
    dma.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    dma.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
    dma.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;
    DMA_Init(RX_DMA_STREAM, &dma);
    USART_DMACmd(CONSOLE_USART, USART_DMAReq_Rx, ENABLE);
    DMA_Cmd(RX_DMA_STREAM, ENABLE);

    /* 5. TX DMA TC 中断 */
    nvic.NVIC_IRQChannel                   = DMA2_Stream7_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    /* 6. 使能串口 */
    USART_Cmd(CONSOLE_USART, ENABLE);

    s_rx_read_pos  = 0;
    s_rx_last_write = 0;
    s_rx_overflow  = 0;
    s_tx_active    = 0;
}

uint8_t Usart_Send(const uint8_t *buf, uint16_t len)
{
    DMA_InitTypeDef dma;

    if (buf == NULL || len == 0)
        return 1;
    if (len > USART_TX_BUF_SIZE)
        return 1;
    if (s_tx_active)
        return 2;   /* 忙, 调用者自行等待 */

    memcpy(s_tx_buf, buf, len);
    s_tx_active = 1;

    DMA_DeInit(TX_DMA_STREAM);
    while (DMA_GetCmdStatus(TX_DMA_STREAM) != DISABLE)
    {
    }

    DMA_ClearFlag(TX_DMA_STREAM,
                  DMA_FLAG_TCIF7 | DMA_FLAG_HTIF7 | DMA_FLAG_TEIF7 |
                  DMA_FLAG_DMEIF7 | DMA_FLAG_FEIF7);

    dma.DMA_Channel            = TX_DMA_CHANNEL;
    dma.DMA_PeripheralBaseAddr = (uint32_t)&CONSOLE_USART->DR;
    dma.DMA_Memory0BaseAddr    = (uint32_t)s_tx_buf;
    dma.DMA_DIR                = DMA_DIR_MemoryToPeripheral;
    dma.DMA_BufferSize         = len;
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode               = DMA_Mode_Normal;
    dma.DMA_Priority           = DMA_Priority_High;
    dma.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    dma.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
    dma.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;
    DMA_Init(TX_DMA_STREAM, &dma);

    USART_DMACmd(CONSOLE_USART, USART_DMAReq_Tx, ENABLE);
    DMA_ITConfig(TX_DMA_STREAM, DMA_IT_TC | DMA_IT_TE | DMA_IT_DME, ENABLE);
    DMA_Cmd(TX_DMA_STREAM, ENABLE);

    return 0;
}

void Usart_Putc(uint8_t ch)
{
    uint32_t start;

    /* 与 DMA TX 互斥, 避免争用发送器; 卡死时超时放弃 */
    start = Systick_GetTick();
    while (s_tx_active)
    {
        if ((Systick_GetTick() - start) >= TX_DRAIN_TIMEOUT_MS)
            break;
    }
    start = Systick_GetTick();
    while (USART_GetFlagStatus(CONSOLE_USART, USART_FLAG_TXE) == RESET)
    {
        if ((Systick_GetTick() - start) >= TX_DRAIN_TIMEOUT_MS)
            return;   /* 发送器无响应, 丢弃该字节避免挂死 */
    }
    USART_SendData(CONSOLE_USART, ch);
}

uint16_t Usart_RxAvailable(void)
{
    uint16_t write_pos = Usart_RxWritePos();
    uint16_t new_bytes = (uint16_t)((write_pos - s_rx_last_write + USART_RX_BUF_SIZE) %
                                    USART_RX_BUF_SIZE);

    if (new_bytes != 0)
    {
        /* 本次新写入的字节数超过了剩余空闲空间, 说明 DMA 环形
         * 回卷已覆盖未读数据, 缓冲内容不可信 */
        uint16_t free_space = (uint16_t)(USART_RX_BUF_SIZE - 1 -
            ((s_rx_last_write - s_rx_read_pos + USART_RX_BUF_SIZE) %
             USART_RX_BUF_SIZE));
        if (new_bytes > free_space)
            s_rx_overflow = 1;
        s_rx_last_write = write_pos;
    }
    return (uint16_t)((write_pos - s_rx_read_pos + USART_RX_BUF_SIZE) %
                      USART_RX_BUF_SIZE);
}

uint8_t Usart_ReadByte(uint8_t *byte)
{
    if (Usart_RxAvailable() == 0)
        return 1;

    *byte = s_rx_buf[s_rx_read_pos];
    s_rx_read_pos = (uint16_t)((s_rx_read_pos + 1) % USART_RX_BUF_SIZE);
    return 0;
}

void Usart_FlushRx(void)
{
    s_rx_read_pos   = Usart_RxWritePos();
    s_rx_last_write = s_rx_read_pos;
}

uint8_t Usart_TakeRxOverflow(void)
{
    uint8_t overflow = s_rx_overflow;
    s_rx_overflow = 0;
    return overflow;
}

void Usart_WaitTxIdle(void)
{
    uint32_t start = Systick_GetTick();

    while (s_tx_active)
    {
        if ((Systick_GetTick() - start) >= TX_DRAIN_TIMEOUT_MS)
        {
            DMA_Cmd(TX_DMA_STREAM, DISABLE);
            DMA_ClearFlag(TX_DMA_STREAM,
                          DMA_FLAG_TCIF7 | DMA_FLAG_HTIF7 | DMA_FLAG_TEIF7 |
                          DMA_FLAG_DMEIF7 | DMA_FLAG_FEIF7);
            s_tx_active = 0;
            break;
        }
    }

    start = Systick_GetTick();
    while (USART_GetFlagStatus(CONSOLE_USART, USART_FLAG_TC) == RESET)
    {
        if ((Systick_GetTick() - start) >= TX_DRAIN_TIMEOUT_MS)
            break;
    }
}

void Usart_DeInit(void)
{
    DMA_Cmd(RX_DMA_STREAM, DISABLE);
    DMA_Cmd(TX_DMA_STREAM, DISABLE);
    USART_DMACmd(CONSOLE_USART, USART_DMAReq_Rx, DISABLE);
    USART_DMACmd(CONSOLE_USART, USART_DMAReq_Tx, DISABLE);
    DMA_DeInit(RX_DMA_STREAM);
    DMA_DeInit(TX_DMA_STREAM);
    USART_Cmd(CONSOLE_USART, DISABLE);
    USART_DeInit(CONSOLE_USART);
    DMA_ITConfig(TX_DMA_STREAM, DMA_IT_TC | DMA_IT_TE | DMA_IT_DME, DISABLE);
    NVIC_DisableIRQ(DMA2_Stream7_IRQn);
    s_tx_active = 0;
}

/* ---- TX DMA 中断 (传输完成 / 错误) ---- */
void DMA2_Stream7_IRQHandler(void)
{
    if (DMA_GetITStatus(TX_DMA_STREAM, DMA_IT_TCIF7) != RESET)
    {
        DMA_ClearITPendingBit(TX_DMA_STREAM, DMA_IT_TCIF7);
        DMA_Cmd(TX_DMA_STREAM, DISABLE);
        while (DMA_GetCmdStatus(TX_DMA_STREAM) != DISABLE)
        {
        }
        USART_ClearFlag(CONSOLE_USART, USART_FLAG_TC);
        s_tx_active = 0;
    }
    else if (DMA_GetITStatus(TX_DMA_STREAM, DMA_IT_TEIF7) != RESET ||
             DMA_GetITStatus(TX_DMA_STREAM, DMA_IT_DMEIF7) != RESET)
    {
        /* 传输错误: 清标志并释放发送器, 避免死锁 */
        DMA_ClearITPendingBit(TX_DMA_STREAM, DMA_IT_TEIF7);
        DMA_ClearITPendingBit(TX_DMA_STREAM, DMA_IT_DMEIF7);
        DMA_Cmd(TX_DMA_STREAM, DISABLE);
        while (DMA_GetCmdStatus(TX_DMA_STREAM) != DISABLE)
        {
        }
        s_tx_active = 0;
    }
}
