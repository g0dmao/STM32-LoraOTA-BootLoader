#include "ota.h"
#include "stddef.h"

int8_t bootOTA_ReadParamOTA(OTA_Context_t *ota_ctx, OTA_Param_t *param)
{
    return ota_ctx->read_cb(ota_ctx->param_address, (uint8_t*)param, sizeof(OTA_Param_t));
}

int8_t bootOTA_SaveParamOTA(OTA_Context_t *ota_ctx, uint32_t size, uint32_t crc, uint32_t magic_flag)
{
    OTA_Param_t new_param = {
        .magic_flag = magic_flag, // 魔术字：代表 App 区有完整固件
        .app_size = size,
        .app_crc = crc,
        .reserved = 0xFFFFFFFF
    };
    if (ota_ctx->erase_cb(ota_ctx->param_sector, ota_ctx->param_sector_num) != 0) {
        return -1; // 擦除失败
    }
    return ota_ctx->write_cb(ota_ctx->param_address, (uint8_t*)&new_param, sizeof(OTA_Param_t));

}


