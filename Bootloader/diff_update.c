#include "diff_update.h"

#include <stdio.h>
#include "configBootloader.h"
#include "janpatch_port.h"

int8_t DiffUpdate_ApplyPatch(uint32_t source_addr,
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

    int ret = JanPatch_Apply(&source_stream, &patch_stream, &target_stream,
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
