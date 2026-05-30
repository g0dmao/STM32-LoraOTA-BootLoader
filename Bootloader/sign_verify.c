#include "sign_verify.h"
#include "configBootloader.h"
#include <string.h>

/* Ed25519 公钥（32 字节），由上位机签名工具生成，编译进 BootLoader */
static const uint8_t ed25519_pubkey[32] = configED25519_PUBKEY;

/**
 * @brief Ed25519 签名校验 — 桩函数
 *
 * 当前为占位实现。启用签名校验后需替换为实际 Ed25519 库
 * （如 libhydrogen 的 hydro_sign_verify 或 tweetnacl-embedded）。
 *
 * @return 0 = 校验通过, -1 = 校验失败
 */
static int8_t ed25519_verify(const uint8_t *signature, const uint8_t *pubkey,
                              const uint8_t *msg, uint32_t msg_len)
{
#if configSIG_VERIFY_ENABLE
    /*
     * TODO: 替换为实际 Ed25519 实现
     *   例: return hydro_sign_verify(signature, msg, msg_len, "ctx", pubkey);
     */
    return -1;
#else
    (void)signature;
    (void)pubkey;
    (void)msg;
    (void)msg_len;
    return 0;
#endif
}

/**
 * @brief 从固件镜像尾部解析 Footer，提取版本号并校验 Ed25519 签名
 */
int8_t bootSIG_ParseAndVerify(uint32_t fw_addr, uint32_t fw_total_size,
                               FW_SignInfo_t *info, uint32_t *fw_bin_size)
{
    FirmwareFooter_t footer;

    /* 1. 检查总大小是否至少能容纳 Footer */
    if (fw_total_size < sizeof(FirmwareFooter_t))
        return -1;

    /* 2. 从镜像尾部读取 Footer */
    memcpy(&footer,
           (void*)(fw_addr + fw_total_size - sizeof(FirmwareFooter_t)),
           sizeof(footer));

    /* 3. 校验 footer_size（自描述） */
    if (footer.footer_size != sizeof(FirmwareFooter_t))
        return -1;

    /* 4. 校验 footer_magic */
    if (footer.footer_magic != configFOOTER_MAGIC)
        return -1;

    /* 5. 固件本体大小 = 总大小 - Footer 大小 */
    *fw_bin_size = fw_total_size - sizeof(FirmwareFooter_t);

    /* 6. 输出版本号和签名 */
    info->version = footer.version;
    memcpy(info->signature, footer.signature, FW_SIGNATURE_SIZE);

    /* 7. Ed25519 签名校验（覆盖固件本体） */
    return ed25519_verify(footer.signature, ed25519_pubkey,
                          (const uint8_t*)fw_addr, *fw_bin_size);
}
