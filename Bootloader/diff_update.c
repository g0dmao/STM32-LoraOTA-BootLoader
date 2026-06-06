#include "diff_update.h"

#include "configBootloader.h"
#include "janpatch_port.h"

int8_t bootDiff_ApplyPatch(uint32_t source_addr,
                           uint32_t target_addr,
                           uint32_t patch_size,
                           uint32_t *fw_size_out)
{
    return JanPatch_ApplyPatch(source_addr, target_addr, patch_size, fw_size_out);
}
