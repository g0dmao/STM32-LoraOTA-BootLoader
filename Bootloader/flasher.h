#ifndef __FLASHER_H__
#define __FLASHER_H__

#include "stdint.h"

int8_t bootFlasher_EraseSectors(int sector, int sector_number);
int8_t bootFlasher_WriteByte(uint32_t address, uint8_t *data, uint16_t length);
int8_t bootFlasher_ReadData(uint32_t address, uint8_t *data, uint16_t length);

#endif




