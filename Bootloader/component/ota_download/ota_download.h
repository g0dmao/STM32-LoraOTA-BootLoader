#ifndef __OTA_DOWNLOAD_H__
#define __OTA_DOWNLOAD_H__

#include <stdint.h>
#include "ymodem.h"
#include "ota.h"

typedef enum OtaDownload_Status
{
    OTADL_STATUS_OK,
    OTADL_STATUS_ERROR
} OtaDownload_Status_t;

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
                                          OTA_Param_t *param);

#endif
