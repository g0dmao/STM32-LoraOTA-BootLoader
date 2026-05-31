#include "flasher.h"
#include <string.h>

#if(!(configUSE_CUSTOM_FLASH))

#include "main.h"

/**
 * @brief  擦除 App 所在的 Flash 扇区 (Sector 3, 4, 5)
 * @retval 0: 成功; -1: 失败
 */
int8_t bootFlasher_EraseSectors(int sector, int sector_number)
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    // 1. 解锁 Flash 控制寄存器
    HAL_FLASH_Unlock();

    // 2. 清除可能存在的错误标志位
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    // 3. 配置擦除参数
    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3; // 2.7V - 3.6V 电压范围
    EraseInitStruct.Sector        = sector;        // 从 Sector 3 开始
    EraseInitStruct.NbSectors     = sector_number;                     // 一共擦除 3 个扇区 (3, 4, 5)

    // 4. 执行擦除
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return -1; // 擦除失败
    }

    // 5. 上锁
    HAL_FLASH_Lock();
    return 0;
}



/**
 * @brief  向目标 Flash 地址写入连续数据
 * @param  address: 写入起始物理地址 (例如 0x0800C000)
 * @param  data: 数据缓冲区指针
 * @param  length: 写入字节数
 * @retval 0: 成功; -1: 失败
 */
int8_t bootFlasher_WriteByte(uint32_t address, uint8_t *data, uint16_t length)
{
    HAL_FLASH_Unlock();

    // 清除标志位，防止被上一次操作的历史错误影响
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    for(uint16_t i = 0; i < length; i++)
    {
        // 逐字节烧写
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + i, data[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1; // 写入出错
        }
    }

    HAL_FLASH_Lock();
    return 0;
}



/**
 * @brief 从 Sector 读取参数
 */
int8_t bootFlasher_ReadData(uint32_t address, uint8_t *data, uint16_t length)
{
    memcpy(data, (void*)address, length);
    return 0;
}

#endif


