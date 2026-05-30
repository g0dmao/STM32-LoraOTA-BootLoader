#include "ota.h"
#include "configBootloader.h"
#include "stddef.h"

int8_t bootOTA_ReadParamOTA(OTA_Context_t *ota_ctx, OTA_Param_t *param)
{
    int8_t ret = ota_ctx->read_cb(ota_ctx->param_address, (uint8_t*)param, sizeof(OTA_Param_t));
    if (ret != 0) return ret;

    // 兼容旧版参数：旧 reserved=0xFFFFFFFF 会使 active_partition 读到 0xFF
    if (param->active_partition > 1)
    {
        param->active_partition = 0;
    }
    return 0;
}

int8_t bootOTA_SaveParamOTA(OTA_Context_t *ota_ctx, uint32_t size, uint32_t crc, uint8_t active_partition)
{
    OTA_Param_t new_param = {
        .magic_flag = configOTA_VALID_MAGIC,
        .app_size   = size,
        .app_crc    = crc,
        .active_partition = active_partition,
        .reserved   = {0xFF, 0xFF, 0xFF}
    };
    if (ota_ctx->erase_cb(ota_ctx->param_sector, ota_ctx->param_sector_num) != 0) {
        return -1;
    }
    return ota_ctx->write_cb(ota_ctx->param_address, (uint8_t*)&new_param, sizeof(OTA_Param_t));
}

uint32_t bootOTA_GetActivePartitionAddr(const OTA_Param_t *param)
{
    return (param->active_partition == 0) ? configPART_A_ADDRESS : configPART_B_ADDRESS;
}

uint32_t bootOTA_GetInactivePartitionAddr(const OTA_Param_t *param)
{
    return (param->active_partition == 0) ? configPART_B_ADDRESS : configPART_A_ADDRESS;
}

void bootOTA_GetInactivePartitionEraseInfo(const OTA_Param_t *param, int *sector, int *sector_num)
{
    if (param->active_partition == 0)
    {
        *sector     = configPART_B_SECTOR;
        *sector_num = configPART_B_SECTOR_NUM;
    }
    else
    {
        *sector     = configPART_A_SECTOR;
        *sector_num = configPART_A_SECTOR_NUM;
    }
}


