#ifndef __UART_DMA_RING_H__
#define __UART_DMA_RING_H__

#include <stdint.h>

void    bootUART_RegisterTransmitPort(void *uart_handle);
uint8_t bootUART_ReadByte(uint8_t *pData);
void    bootUART_SendByte(uint8_t data);

#endif