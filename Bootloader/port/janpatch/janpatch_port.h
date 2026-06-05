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
 * @brief  应用 JANPatch 补丁：source + patch → target
 * @param  source       源固件 FlashStream（活跃分区，只读）
 * @param  patch        补丁 FlashStream（Sector 4，只读）
 * @param  target       目标固件 FlashStream（非活跃分区，已擦除，写入）
 * @param  fw_size_out  输出：生成的新固件总大小（含 Footer），可为 NULL
 * @return 0=成功, 非0=janpatch 错误码
 */
int JanPatch_Apply(FlashStream_t *source,
                   FlashStream_t *patch,
                   FlashStream_t *target,
                   uint32_t *fw_size_out);

#endif
