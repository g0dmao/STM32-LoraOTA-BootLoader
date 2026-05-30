#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

#include <stdint.h>

void Bootloader_JumpToApp(uint32_t app_address);

#endif