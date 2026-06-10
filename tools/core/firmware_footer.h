/**
 * @file    firmware_footer.h
 * @brief   固件 Footer 共享定义 — keygen/binpkg 与 BootLoader 共用
 *
 * 本文件与 BootLoader/Bootloader/sign_verify.h 和 configBootloader.h 中的
 * 定义保持一致。任何修改需同步两端。
 *
 * Footer 内存布局（小端序，共 76 字节）：
 *   偏移 0:  version      (uint32_t, 4B)
 *   偏移 4:  signature    (uint8_t[64], 64B)
 *   偏移 68: footer_magic (uint32_t, 4B) = 0xAA55F00D
 *   偏移 72: footer_size  (uint32_t, 4B) = 76
 */

#ifndef FIRMWARE_FOOTER_H
#define FIRMWARE_FOOTER_H

#include <stdint.h>

/* ============================================================
 *  常量定义
 * ============================================================ */

/** Ed25519 签名长度（字节），对应 hydro_sign_BYTES */
#define FW_SIGNATURE_SIZE  64

/** Footer 魔数，用于校验 footer 有效性 */
#define FW_FOOTER_MAGIC    0xAA55F00D

/**
 * Ed25519 签名上下文（8 字节，含 '\0'）
 *
 * 上位机签名工具和 BootLoader 验签时必须使用完全相同的上下文字符串，
 * 否则签名校验将失败。
 */
#define FW_SIGN_CONTEXT    "114_514"

/** Footer 结构体大小（字节） */
#define FW_FOOTER_SIZE     76

/* ============================================================
 *  数据结构
 * ============================================================ */

/**
 * @brief 固件尾部元数据结构（Ed25519 签名 + 版本号）
 *
 * 布局（从尾到头）：
 *   footer_size  (4B)  ← 总大小 76，自描述
 *   footer_magic (4B)  ← 0xAA55F00D
 *   signature   (64B)  ← Ed25519 签名，覆盖 firmware binary
 *   version      (4B)  ← 固件版本号，单调递增
 *
 * 签名覆盖范围：firmware binary = fw[0 .. fw_total_size - 76]
 */
typedef struct __attribute__((packed)) {
    uint32_t version;
    uint8_t  signature[FW_SIGNATURE_SIZE];
    uint32_t footer_magic;
    uint32_t footer_size;
} FirmwareFooter_t;

/* 编译期断言：确保结构体大小为 76 字节 */
_Static_assert(sizeof(FirmwareFooter_t) == FW_FOOTER_SIZE,
               "FirmwareFooter_t size mismatch, expected 76 bytes");

#endif /* FIRMWARE_FOOTER_H */
