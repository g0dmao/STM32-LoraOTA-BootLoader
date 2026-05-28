#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

#include "main.h"

typedef void (*pFunction)(void);
void Bootloader_JumpToApp(pFunction CloseAllPeripheralFuc);

#endif