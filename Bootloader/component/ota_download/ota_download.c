#include "ota_download.h"
#include "configBootloader.h"
#include "sign_verify.h"
#include "diff_update.h"
#include <stdio.h>

/**
 * @brief  检测文件名是否包含 "_patch"，判断是否为差量更新包
 *
 * @param  file_name  文件名（来自 Ymodem 第 0 包）
 * @return 1  差量包
 *         0  全量包
 */
static uint8_t IsPatchMode_(const char *file_name)
{
    const char *p = file_name;

    while (*p)
    {
        if (p[0] == '_' && p[1] == 'p' && p[2] == 'a' &&
            p[3] == 't' && p[4] == 'c' && p[5] == 'h')
        {
            return 1;
        }
        p++;
    }
    return 0;
}

/**
 * @brief  建立 Ymodem 连接并擦除目标 Flash 区域
 *
 * @param  ym          Ymodem 上下文
 * @param  ota_ctx     OTA 上下文（Flash 操作回调）
 * @param  param       OTA 参数
 * @param  is_patch    输出：是否为差量模式
 * @param  write_addr  输出：写入目标地址
 * @return 0  成功
 *        -1  失败
 */
static int8_t ConnectAndErase_(YM_InfoBlock_t *ym, OTA_Context_t *ota_ctx,
                                OTA_Param_t *param, uint8_t *is_patch,
                                uint32_t *write_addr)
{
    int sector = 0;
    int sector_num = 0;

    for (;;)
    {
        if (bootYM_EstablishConnection(ym) == YM_RETURN_CODE_OK)
        {
            *is_patch = IsPatchMode_(ym->file_name);

            if (*is_patch)
            {
                if (ota_ctx->erase_cb(configPATCH_STORAGE_SECTOR,
                                       configPATCH_STORAGE_SECTOR_NUM) != 0)
                {
                    bootYM_Abort(ym);
                    return -1;
                }
                *write_addr = configPATCH_STORAGE_ADDRESS;
            }
            else
            {
                bootOTA_GetInactivePartitionEraseInfo(param, &sector, &sector_num);
                if (ota_ctx->erase_cb(sector, sector_num) != 0)
                {
                    bootYM_Abort(ym);
                    return -1;
                }
                *write_addr = bootOTA_GetInactivePartitionAddr(param);
            }
            return 0;
        }
    }
}

/**
 * @brief  接收 Ymodem 数据包并写入 Flash，完成后进行签名校验、防回滚检查、CRC 校验
 *
 * @param  ym          Ymodem 上下文
 * @param  ota_ctx     OTA 上下文
 * @param  param       OTA 参数（输出更新后的分区和版本号）
 * @param  write_addr  写入基地址
 * @param  is_patch    是否为差量模式
 * @return 0  成功
 *        -1  失败
 */
static int8_t ReceiveAndFlash_(YM_InfoBlock_t *ym, OTA_Context_t *ota_ctx,
                                OTA_Param_t *param, uint32_t write_addr,
                                uint8_t is_patch)
{
    int      sector = 0;
    int      sector_num = 0;
    uint32_t write_offset = 0;

    for (;;)
    {
        int8_t ret = bootYM_AccepctOnePacket(ym);

        if (ret == YM_RETURN_CODE_OK)
        {
            write_offset = ym->total_receive_byte - ym->packet_len;
            if (ota_ctx->write_cb(write_addr + write_offset, ym->packet_data,
                                   ym->packet_len) != 0)
            {
                bootYM_Abort(ym);
                return -1;
            }
        }
        else if (ret == YM_RETURN_CODE_EOT)
        {
            uint32_t fw_bin_size = 0;
            uint32_t fw_total_size;
            FW_SignInfo_t sign_info;

            if (is_patch)
            {
                uint32_t src_addr = bootOTA_GetActivePartitionAddr(param);
                uint32_t dst_addr = bootOTA_GetInactivePartitionAddr(param);

                bootOTA_GetInactivePartitionEraseInfo(param, &sector, &sector_num);
                if (ota_ctx->erase_cb(sector, sector_num) != 0)
                {
                    bootYM_Abort(ym);
                    return -1;
                }

                ota_ctx->unlock_cb();
                int8_t patch_ret = bootDiff_ApplyPatch(src_addr, dst_addr,
                                                        ym->file_size, &fw_total_size);
                ota_ctx->lock_cb();

                if (patch_ret != 0)
                {
                    bootYM_Abort(ym);
                    return -1;
                }

                write_addr = dst_addr;
            }
            else
            {
                fw_total_size = ym->file_size;
            }

#if(configUSE_FOOTER)
            {
                int8_t sig_ret = bootSIG_ParseAndVerify(write_addr, fw_total_size,
                                                        &sign_info, &fw_bin_size);
                if (sig_ret != 0)
                {
                    printf("SIG_ERR: %d\r\n", sig_ret);
                    bootYM_Abort(ym);
                    return -1;
                }

#if(!configROLLBACK_ENABLE)
                if (sign_info.version < param->current_version)
                {
                    printf("ROLLBACK: v%lu < v%lu\r\n", sign_info.version,
                           param->current_version);
                    bootYM_Abort(ym);
                    return -1;
                }
#endif
            }
#else
            sign_info.version = 0xFFFFFFFF;
#endif

            OTA_Param_t new_param = {
                .app_size         = fw_bin_size,
                .app_crc          = CalcCRC16((uint8_t *)write_addr, fw_bin_size),
                .active_partition = (uint8_t)((param->active_partition == 0) ? 1 : 0),
                .current_version  = sign_info.version,
                .reserved         = {0xFF, 0xFF, 0xFF}
            };

            bootOTA_SaveParamOTA(ota_ctx, &new_param);

            param->active_partition = new_param.active_partition;
            param->current_version  = new_param.current_version;
            return 0;
        }
        else
        {
            bootYM_Abort(ym);
            return -1;
        }
    }
}

/**
 * @brief  执行完整的 OTA 下载流程
 *
 *         封装 Ymodem 连接建立、全量/差量模式检测、Flash 擦除、
 *         数据包接收与写入、差量补丁应用、Ed25519 签名校验、
 *         防回滚检查、CRC 校验和 OTA 参数保存。
 *
 *         调用前需确保：
 *         1. ym 的 read_byte_cb / send_byte_cb / get_tick_cb 已注册
 *         2. ota_ctx 的 read_cb / write_cb / erase_cb / unlock_cb / lock_cb 已注册
 *
 * @param  ym      Ymodem 上下文控制块（含已注册 I/O 回调）
 * @param  ota_ctx OTA 上下文控制块（含已注册 Flash 驱动回调）
 * @param  param   输入输出：当前 OTA 参数，成功后更新至新的分区和版本信息
 * @return OTADL_STATUS_OK    下载成功，param 已更新，调用方应跳转 APP
 *         OTADL_STATUS_ERROR 下载失败
 */
OtaDownload_Status_t OtaDownload_Execute(YM_InfoBlock_t *ym,
                                          OTA_Context_t *ota_ctx,
                                          OTA_Param_t *param)
{
    uint8_t  is_patch = 0;
    uint32_t write_addr = 0;

    if (ConnectAndErase_(ym, ota_ctx, param, &is_patch, &write_addr) != 0)
    {
        return OTADL_STATUS_ERROR;
    }

    if (ReceiveAndFlash_(ym, ota_ctx, param, write_addr, is_patch) != 0)
    {
        return OTADL_STATUS_ERROR;
    }

    return OTADL_STATUS_OK;
}
