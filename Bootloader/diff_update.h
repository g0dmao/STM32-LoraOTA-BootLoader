#ifndef __DIFF_UPDATE_H__
#define __DIFF_UPDATE_H__

#include <stdint.h>

/**
 * @brief  将 Sector 4 中的补丁应用到源固件，生成新固件到目标分区
 *
 *         调用前需确保：
 *         1. 补丁文件已通过 Ymodem 接收完毕并写入 Sector 4
 *         2. 目标分区已擦除（由调用方负责）
 *
 * @param  source_addr   源固件基地址（活跃分区）
 * @param  target_addr   目标固件基地址（非活跃分区，已擦除）
 * @param  patch_size    补丁文件大小（字节）
 * @param  fw_size_out   输出：生成的新固件总大小（含 Footer）
 * @return 0=成功, -1=janpatch错误
 */
int8_t bootDiff_ApplyPatch(uint32_t source_addr,
                           uint32_t target_addr,
                           uint32_t patch_size,
                           uint32_t *fw_size_out);

#endif
