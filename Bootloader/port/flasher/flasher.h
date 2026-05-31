#ifndef __FLASHER_H__
#define __FLASHER_H__

#include "stdint.h"

/**
 * @brief 使用HAL库擦除主flash-封装
 *
 * @param sector 要擦除的第一个分区
 * @param sector_number 从第一个分区开始往后擦几个分区
 * @return int8_t 0：成功
 */
int8_t bootFlasher_EraseSectors(int sector, int sector_number);

/**
 * @brief 使用HAL库写入主flash-封装
 *
 * @param address 写入的地址
 * @param data    写入的数据
 * @param length  数据长度
 * @return int8_t 0：成功
 */
int8_t bootFlasher_WriteByte(uint32_t address, uint8_t *data, uint16_t length);

/**
 * @brief 使用HAL库读主flash-封装
 *
 * @param address 要读的地址
 * @param data    读出数据缓存区
 * @param length  数据长度
 * @return int8_t 0：成功
 */
int8_t bootFlasher_ReadData(uint32_t address, uint8_t *data, uint16_t length);

#endif




