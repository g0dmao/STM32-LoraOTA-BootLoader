#include "uart_dma_ring.h"
#include "configBootloader.h"

#if(configUART)
#include "main.h"

/* 全局 tick 计数器，在 SysTick_Handler 中递增，替代 HAL_GetTick */
volatile uint32_t g_sys_tick = 0;

/* DMA 硬件直接写入的物理缓冲区 */
uint8_t rx_buffer[configRX_BUF_SIZE] = {0};

/* 读写指针 */
volatile uint16_t rx_write_ptr = 0; /* DMA 当前写到哪了 */
volatile uint16_t rx_read_ptr  = 0; /* 我们目前读到哪了 */

/**
 * @brief  注册传输端口，启动 DMA 循环接收 + USART IDLE 中断
 * @note   使用 LL 库直接操作 USART1 和 DMA2 Stream2
 */
void bootUART_RegisterTransmitPort(void)
{
    /* 配置 DMA 外设/内存地址及传输长度 */
    LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_2, (uint32_t)&USART1->DR);
    LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_2, (uint32_t)rx_buffer);
    LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_2, configRX_BUF_SIZE);

    /* 开启 USART DMA 接收请求 */
    LL_USART_EnableDMAReq_RX(USART1);

    /* 开启 USART IDLE 中断（帧结束检测） */
    LL_USART_EnableIT_IDLE(USART1);

    /* 启动 DMA 流（循环模式已在 MX_USART1_UART_Init 中配置） */
    LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_2);
}

/**
 * @brief  从环形缓冲区中读取一个字节的数据
 * @param  pData: 读取数据的存放地址
 * @retval 1: 读取成功; 0: 缓冲区空，无数据可读
 */
uint8_t bootUART_ReadByte(uint8_t *pData)
{
    /* 实时更新写指针：缓冲区大小 - DMA 剩余传输长度 */
    rx_write_ptr = configRX_BUF_SIZE - LL_DMA_GetDataLength(DMA2, LL_DMA_STREAM_2);

    /* 当 DMA 计数器归零瞬间，写指针会等于 configRX_BUF_SIZE，回绕到 0 */
    if (rx_write_ptr >= configRX_BUF_SIZE)
    {
        rx_write_ptr = 0;
    }

    /* 如果读指针等于写指针，说明目前没有新数据 */
    if (rx_read_ptr == rx_write_ptr)
    {
        return 0; /* 没数据 */
    }

    /* 拿出一个字节 */
    *pData = rx_buffer[rx_read_ptr];

    /* 读指针加 1，如果到了数组尾部，就绕回头部 */
    rx_read_ptr++;
    if (rx_read_ptr >= configRX_BUF_SIZE)
    {
        rx_read_ptr = 0;
    }

    return 1; /* 成功读到一个字节 */
}

/**
 * @brief  通过 USART1 阻塞发送一个字节
 */
void bootUART_SendByte(uint8_t data)
{
    while (!LL_USART_IsActiveFlag_TXE(USART1));
    LL_USART_TransmitData8(USART1, data);
}

#endif
