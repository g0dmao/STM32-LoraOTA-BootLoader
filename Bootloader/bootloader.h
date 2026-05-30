#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

#include <stdint.h>

/**
 * @brief 核心跳转函数
 *
 * @param app_address 要跳转的app分区地址
 */
void Bootloader_JumpToApp(uint32_t app_address);

#endif