#include "uart_dma_ring.h"
#include "configBootloader.h"

#if(configUART)
#include "main.h"

UART_HandleTypeDef *g_uart_handle = NULL;

// DMA 硬件直接写入的物理缓冲区
uint8_t rx_buffer[configRX_BUF_SIZE] = {0};

// 读写指针
volatile uint16_t rx_write_ptr = 0; // DMA 当前写到哪了
volatile uint16_t rx_read_ptr  = 0;  // 我们目前读到哪了

void bootUART_RegisterTransmitPort(void *uart_handle)
{
    g_uart_handle = (UART_HandleTypeDef*)uart_handle;
    // 调用扩展 API，开启 DMA 接收并自动关联 IDLE 中断
    HAL_UARTEx_ReceiveToIdle_DMA(g_uart_handle, rx_buffer, configRX_BUF_SIZE);
}

/**
 * @brief  从环形缓冲区中读取一个字节的数据
 * @param  pData: 读取数据的存放地址
 * @retval 1: 读取成功; 0: 缓冲区空，无数据可读
 */
uint8_t bootUART_ReadByte(uint8_t *pData)
{
    // 实时更新写指针
    rx_write_ptr = configRX_BUF_SIZE - __HAL_DMA_GET_COUNTER(g_uart_handle->hdmarx);

    // 当 DMA 计数器归零瞬间，写指针会等于 configRX_BUF_SIZE，回绕到 0
    if (rx_write_ptr >= configRX_BUF_SIZE)
    {
        rx_write_ptr = 0;
    }

    // 如果读指针等于写指针，说明目前没有新数据
    if (rx_read_ptr == rx_write_ptr)
    {
        return 0; // 没数据
    }

    // 拿出一个字节
    *pData = rx_buffer[rx_read_ptr];

    // 读指针加 1，如果到了数组尾部，就绕回头部
    rx_read_ptr++;
    if (rx_read_ptr >= configRX_BUF_SIZE)
    {
        rx_read_ptr = 0;
    }

    return 1; // 成功读到一个字节
}

void bootUART_SendByte(uint8_t data)
{
    HAL_UART_Transmit(g_uart_handle, &data, 1, HAL_MAX_DELAY);
}

// 定义回调函数
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == g_uart_handle);

}


#endif
