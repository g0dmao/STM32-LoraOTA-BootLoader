#ifndef __OTA_H__
#define __OTA_H__

#include <stdint.h>


typedef struct OTA_Param{
    uint32_t magic_flag;        // 升级成功标志位
    uint32_t app_size;          // 固件实际大小
    uint32_t app_crc;           // 固件的整体 CRC 校验值
    uint8_t  active_partition;  // 当前活跃分区: 0 = Partition A, 1 = Partition B
    uint8_t  reserved[3];       // 保留位,对齐到 16 字节，方便 Flash 写入
} OTA_Param_t;

typedef struct OTA_Context{
    uint32_t param_address;      // 参数区地址
    uint32_t param_sector;      // 参数区所在的扇区号
    uint32_t param_sector_num;  // 扇区数

    // 底层驱动注册
    int8_t (*write_cb)(uint32_t address, uint8_t *data, uint16_t length);
    int8_t (*read_cb)(uint32_t address, uint8_t *data, uint16_t length);
    int8_t (*erase_cb)(int sector, int sector_number);
} OTA_Context_t;

int8_t bootOTA_ReadParamOTA(OTA_Context_t *ota_ctx, OTA_Param_t *param);
int8_t bootOTA_SaveParamOTA(OTA_Context_t *ota_ctx, uint32_t size, uint32_t crc, uint8_t active_partition);

uint32_t bootOTA_GetActivePartitionAddr(const OTA_Param_t *param);
uint32_t bootOTA_GetInactivePartitionAddr(const OTA_Param_t *param);
void     bootOTA_GetInactivePartitionEraseInfo(const OTA_Param_t *param, int *sector, int *sector_num);

#endif

