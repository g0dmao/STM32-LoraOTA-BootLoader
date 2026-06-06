#ifndef __JANPATCH_PORT_H__
#define __JANPATCH_PORT_H__

#include <stdint.h>
#include <stdio.h>

/**
 * @brief  Flash 流描述符，用于 JANPatch 访问 Flash 上的数据
 *
 * JANPatch 通过此结构体访问源固件、补丁文件和目标固件。
 * start_address: Flash 基地址（内存映射，直接可读）
 * current_offset: 当前读/写偏移
 * max_size: 最大可访问字节数
 */
typedef struct FlashStream
{
    uint32_t start_address;
    uint32_t current_offset;
    uint32_t max_size;
} FlashStream_t;

/* 在包含 janpatch.h 之前定义 JANPATCH_STREAM，使 janpatch.h 适配裸机环境 */
#define JANPATCH_STREAM FlashStream_t

/**
 * @brief  将 Sector 4 中的补丁应用到源固件，生成新固件到目标分区
 *
 * @note   调用前需确保：
 *         1. 补丁文件已通过 Ymodem 接收完毕并写入 Sector 4
 *         2. 目标分区已擦除（由调用方负责）
 *
 * @param  source_addr   源固件基地址（活跃分区）
 * @param  target_addr   目标固件基地址（非活跃分区，已擦除）
 * @param  patch_size    补丁文件大小（字节）
 * @param  fw_size_out   输出：生成的新固件总大小（含 Footer）
 * @return 0=成功, -1=janpatch错误
 */
int8_t JanPatch_ApplyPatch(uint32_t source_addr,
                             uint32_t target_addr,
                             uint32_t patch_size,
                             uint32_t *fw_size_out);

#endif
