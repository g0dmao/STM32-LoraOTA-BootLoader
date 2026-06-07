
#include "sign_verify.h"
#include "configBootloader.h"
#include "hydrogen.h"
#include <string.h>
//#include <stdio.h>

#if(configUSE_FOOTER)

/* Ed25519 公钥（32 字节），由上位机签名工具生成，编译进 BootLoader */
static const uint8_t ed25519_pubkey[32] = configED25519_PUBKEY;

/**
 * @brief 签名验证函数
 *
 * @param signature 输入-数字签名
 * @param pubkey    输入-公匙
 * @param msg       输入-原始数据地址
 * @param msg_len   输入-原始数据长度
 * @return int8_t
 */
static int8_t ed25519_verify(const uint8_t *signature, const uint8_t *pubkey,
                              const uint8_t *msg, uint32_t msg_len)
{
#if configSIG_VERIFY_ENABLE
    /*
     * TODO: 替换为实际 Ed25519 实现
     *   例: return hydro_sign_verify(signature, msg, msg_len, "ctx", pubkey);
     */
    // 根据hydro文档，“a context is a 8 characters string”，所以ctx必须为8个（及以上？？没试过）字符（含‘\0’）
    // 上位机签名工具中也必须使用完全相同的 8 字节字符串

    return hydro_sign_verify(signature, msg, msg_len, "114_514", pubkey);
#else
    // 消除变量未使用警告
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

    // {
    //     uint32_t footer_addr = fw_addr + fw_total_size - sizeof(FirmwareFooter_t);

    //     /* DEBUG: 打印 Footer 区域原始字节 */
    //     printf("FOOTER_DBG: fw=0x%08lX total=%lu footer@0x%08lX\r\n",
    //            (unsigned long)fw_addr, (unsigned long)fw_total_size,
    //            (unsigned long)footer_addr);
    //     {
    //         const uint8_t *raw = (const uint8_t *)footer_addr;
    //         printf("  raw[0..75]: ");
    //         for (int i = 0; i < (int)sizeof(FirmwareFooter_t); i++)
    //         {
    //             printf("%02X ", raw[i]);
    //             if ((i + 1) % 16 == 0 && i < 75) printf("\r\n              ");
    //         }
    //         printf("\r\n");
    //     }

    //     memcpy(&footer, (void*)footer_addr, sizeof(footer));
    //     printf("  version=%lu(0x%08lX) magic=0x%08lX(exp 0x%08lX) size=%lu(exp %u)\r\n",
    //            (unsigned long)footer.version, (unsigned long)footer.version,
    //            (unsigned long)footer.footer_magic, (unsigned long)configFOOTER_MAGIC,
    //            (unsigned long)footer.footer_size, (unsigned int)sizeof(FirmwareFooter_t));
    // }

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

#endif