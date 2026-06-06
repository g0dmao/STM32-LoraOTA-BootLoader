#include "janpatch_port.h"
#include "janpatch.h"
#include "flasher.h"
#include "configBootloader.h"
#include <string.h>

/* ---- 静态页缓冲区 ---- */

static uint8_t s_source_page_buf[configJP_SOURCE_PAGE_SIZE];
static uint8_t s_patch_page_buf[configJP_PATCH_PAGE_SIZE];
static uint8_t s_target_page_buf[configJP_TARGET_PAGE_SIZE];

/* ---- 静态 Flash 流 I/O 回调 ---- */

/**
 * @brief  从 Flash 流中读取数据
 */
static size_t FlashStream_Read_(void *ptr, size_t size, size_t count,
                                FlashStream_t *stream)
{
    uint32_t addr = stream->start_address + stream->current_offset;
    size_t   total = size * count;

    if (stream->current_offset + total > stream->max_size)
    {
        total = stream->max_size - stream->current_offset;
    }

    if (total == 0)
    {
        return 0;
    }

    memcpy(ptr, (void *)addr, total);
    stream->current_offset += total;

    return total / size;
}

/**
 * @brief  向 Flash 流中写入数据（使用无锁写入）
 */
static size_t FlashStream_Write_(const void *ptr, size_t size, size_t count,
                                  FlashStream_t *stream)
{
    uint32_t addr = stream->start_address + stream->current_offset;
    size_t   total = size * count;

    if (stream->current_offset + total > stream->max_size)
    {
        total = stream->max_size - stream->current_offset;
    }

    if (total == 0)
    {
        return 0;
    }

    if (bootFlasher_Write(addr, (uint8_t *)ptr, (uint16_t)total) != 0)
    {
        return 0;
    }

    stream->current_offset += total;

    return total / size;
}

/**
 * @brief  在 Flash 流中定位（仅支持 SEEK_SET 和 SEEK_CUR）
 */
static int FlashStream_Seek_(FlashStream_t *stream, long int offset, int origin)
{
    if (origin == SEEK_SET)
    {
        stream->current_offset = (uint32_t)offset;
    }
    else if (origin == SEEK_CUR)
    {
        stream->current_offset += (uint32_t)offset;
    }
    else
    {
        return -1;
    }

    if (stream->current_offset > stream->max_size)
    {
        stream->current_offset = stream->max_size;
    }

    return 0;
}

/**
 * @brief  返回 Flash 流当前偏移
 */
static long FlashStream_Tell_(FlashStream_t *stream)
{
    return (long)stream->current_offset;
}

/**
 * @brief  初始化 janpatch_ctx，绑定 Flash I/O 回调和页缓冲区
 */
static void JanPatch_Init_(janpatch_ctx *ctx,
                           FlashStream_t *source,
                           FlashStream_t *patch,
                           FlashStream_t *target)
{
    ctx->fread  = FlashStream_Read_;
    ctx->fwrite = FlashStream_Write_;
    ctx->fseek  = FlashStream_Seek_;
    ctx->ftell  = FlashStream_Tell_;

    ctx->source_buffer.buffer = s_source_page_buf;
    ctx->source_buffer.size   = configJP_SOURCE_PAGE_SIZE;
    ctx->source_buffer.stream = source;

    ctx->patch_buffer.buffer  = s_patch_page_buf;
    ctx->patch_buffer.size    = configJP_PATCH_PAGE_SIZE;
    ctx->patch_buffer.stream  = patch;

    ctx->target_buffer.buffer = s_target_page_buf;
    ctx->target_buffer.size   = configJP_TARGET_PAGE_SIZE;
    ctx->target_buffer.stream = target;

    ctx->progress      = NULL;
    ctx->max_file_size = 0;
}

/**
 * @brief  应用 JANPatch 补丁：source + patch → target
 * @param  source       源固件 FlashStream（活跃分区，只读）
 * @param  patch        补丁 FlashStream（Sector 4，只读）
 * @param  target       目标固件 FlashStream（非活跃分区，已擦除，写入）
 * @param  fw_size_out  输出：生成的新固件总大小（含 Footer），可为 NULL
 * @return 0=成功, 非0=janpatch 错误码
 */
static int JanPatch_Apply_(FlashStream_t *source,
                   FlashStream_t *patch,
                   FlashStream_t *target,
                   uint32_t *fw_size_out)
{
    janpatch_ctx ctx;

    JanPatch_Init_(&ctx, source, patch, target);

    int ret = janpatch(ctx, source, patch, target);

    if (ret == 0 && fw_size_out != NULL)
    {
        /* 返回总字节数。 */
        *fw_size_out = target->current_offset;
    }

    return ret;
}


/* ---- 公共函数 ---- */

int8_t JanPatch_ApplyPatch(uint32_t source_addr,
                             uint32_t target_addr,
                             uint32_t patch_size,
                             uint32_t *fw_size_out)
{
    FlashStream_t source_stream = {
        .start_address  = source_addr,
        .current_offset = 0,
        .max_size       = configAPP_MAX_SIZE
    };

    FlashStream_t patch_stream = {
        .start_address  = configPATCH_STORAGE_ADDRESS,
        .current_offset = 0,
        .max_size       = patch_size
    };

    FlashStream_t target_stream = {
        .start_address  = target_addr,
        .current_offset = 0,
        .max_size       = configAPP_MAX_SIZE
    };

    int ret = JanPatch_Apply_(&source_stream, &patch_stream, &target_stream,
                             fw_size_out);
    if (ret != 0)
    {
        printf("DIFF_ERR: janpatch returned %d\r\n", ret);
        printf("  source=%lu patch=%lu target=%lu\r\n",
               (unsigned long)source_stream.current_offset,
               (unsigned long)patch_stream.current_offset,
               (unsigned long)target_stream.current_offset);
        return -1;
    }


    return 0;
}
