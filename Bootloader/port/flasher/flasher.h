#ifndef __FLASHER_H__
#define __FLASHER_H__

#include "stdint.h"

/**
 * @brief  解锁 Flash 控制寄存器（KEYR 序列）
 * @note   在批量写入前调用一次，配合 bootFlasher_Write 使用
 */
void bootFlasher_Unlock(void);

/**
 * @brief  上锁 Flash 控制寄存器（CR.LOCK = 1）
 * @note   在批量写入完成后调用一次
 */
void bootFlasher_Lock(void);

/**
 * @brief  擦除 App 所在的 Flash 扇区
 * @param  sector:        起始扇区号 (0-11)
 * @param  sector_number: 连续擦除的扇区个数
 * @retval 0: 成功; -1: 失败
 */
int8_t bootFlasher_EraseSectors(int sector, int sector_number);

/**
 * @brief  向目标 Flash 地址写入连续数据（便捷封装，内部自动解锁/上锁）
 * @param  address: 写入起始物理地址
 * @param  data:    数据缓冲区指针
 * @param  length:  写入字节数
 * @retval 0: 成功; -1: 失败
 */
int8_t bootFlasher_WriteByte(uint32_t address, uint8_t *data, uint16_t length);

/**
 * @brief  向目标 Flash 地址写入连续数据（不含解锁/上锁，需外部管理）
 * @param  address: 写入起始物理地址
 * @param  data:    数据缓冲区指针
 * @param  length:  写入字节数
 * @retval 0: 成功; -1: 失败
 * @note   调用前必须已调用 bootFlasher_Unlock()，完成后调用 bootFlasher_Lock()
 */
int8_t bootFlasher_Write(uint32_t address, uint8_t *data, uint16_t length);

/**
 * @brief  从 Flash 地址读取数据（Flash 内存映射，直接 memcpy）
 * @param  address: 读取起始物理地址
 * @param  data:    读出数据缓冲区
 * @param  length:  读取字节数
 * @retval 0: 成功
 */
int8_t bootFlasher_ReadData(uint32_t address, uint8_t *data, uint16_t length);

#endif




