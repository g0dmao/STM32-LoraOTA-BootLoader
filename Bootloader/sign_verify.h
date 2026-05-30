#ifndef __SIGN_VERIFY_H__
#define __SIGN_VERIFY_H__

#include <stdint.h>

#define FW_SIGNATURE_SIZE  64

/**
 * @brief 固件尾部元数据结构（Ed25519 签名 + 版本号）
 *
 * 布局（从尾到头）：
 *   footer_size  (4B)  ← 总大小 76，自描述
 *   footer_magic (4B)  ← 0xAA55F00D
 *   signature   (64B)  ← Ed25519 签名，覆盖 firmware binary
 *   version      (4B)  ← 固件版本号，单调递增
 *
 * 签名覆盖范围：firmware binary = fw_addr[0 .. fw_total_size - 76]
 */
typedef struct __attribute__((packed)) {
    uint32_t version;
    uint8_t  signature[FW_SIGNATURE_SIZE];
    uint32_t footer_magic;
    uint32_t footer_size;
} FirmwareFooter_t;

/**
 * @brief 从 Footer 提取的信息
 */
typedef struct {
    uint32_t version;
    uint8_t  signature[FW_SIGNATURE_SIZE];
} FW_SignInfo_t;

/**
 * @brief 从固件镜像尾部解析 Footer，提取版本号并校验 Ed25519 签名
 *
 * @param fw_addr       固件在 Flash 中的基地址
 * @param fw_total_size 固件总大小（Ymodem 传输的 file_size，含 Footer）
 * @param info          输出：版本号 + 签名
 * @param fw_bin_size   输出：固件本体大小（不含 Footer）
 * @return  0 = 签名校验通过
 *         -1 = Footer 格式错误（size/magic 不匹配或镜像过小）
 *         -2 = Ed25519 签名校验失败
 */
int8_t bootSIG_ParseAndVerify(uint32_t fw_addr, uint32_t fw_total_size,
                               FW_SignInfo_t *info, uint32_t *fw_bin_size);

#endif
