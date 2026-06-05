#ifndef __CONFIG_BOOTLOADER_H__
#define __CONFIG_BOOTLOADER_H__

#define Yes                1
#define No                 0

// ============================================================
// A/B 双分区 OTA 布局
// ============================================================
// BootLoader:   Sector 0-1   (32KB, 0x08000000)
// OTA Params:   Sector 2     (16KB, 0x08008000)
// Free:         Sector 3     (16KB, 0x0800C000)  预留扩展
// Patch Store:  Sector 4     (64KB, 0x08010000)  补丁暂存区
// Partition B:  Sector 5     (128KB,0x08020000)  App 分区 B
// Partition A:  Sector 6     (128KB,0x08040000)  默认 / 出厂分区
// Reserved:     Sector 7     (128KB,0x08060000)  预留
// ============================================================

// --- Partition A ---
#define configPART_A_ADDRESS       0x08040000
#define configPART_A_SECTOR        6
#define configPART_A_SECTOR_NUM    1     // Sector 6 (128KB)

// --- Partition B ---
#define configPART_B_ADDRESS       0x08020000
#define configPART_B_SECTOR        5
#define configPART_B_SECTOR_NUM    1     // Sector 5 (128KB)

// --- App 固件大小约束 ---
#define configAPP_MAX_SIZE         (128 * 1024)

// --- OTA 参数区 ---
#define configPARAM_ADDRESS        0x08008000
#define configPARAM_SECTOR         2
#define configPARAM_SECTOR_NUM     1
#define configOTA_VALID_MAGIC      0x55AA55AA

// --- Patch 暂存区 ---
#define configPATCH_STORAGE_ADDRESS    0x08010000
#define configPATCH_STORAGE_SECTOR     4
#define configPATCH_STORAGE_SECTOR_NUM 1
#define configPATCH_MAX_SIZE           (64 * 1024)

// --- JANPatch 页缓冲区大小 ---
#define configJP_SOURCE_PAGE_SIZE      4096
#define configJP_PATCH_PAGE_SIZE       4096
#define configJP_TARGET_PAGE_SIZE      4096


#define configUART         Yes
#if(configUART)
    #define configRX_BUF_SIZE        2048
#endif

#define configUSE_CUSTOM_FLASH       No

#define configMS_TO_JUMP             3000
// ============================================================
// 固件签名校验（Ed25519）
// ============================================================

// 如果只想传裸固件，不附加footer，请改为No
// 这会忽略签名校验和版本检查（防回滚）
#define configUSE_FOOTER           Yes

// 签名校验开关：1 = 强制验签，0 = 跳过（调试用）
#define configSIG_VERIFY_ENABLE    Yes

// 防回滚开关：1 = 启动防回滚校验，0 = 跳过（调试用）
#define configROLLBACK_ENABLE      Yes

// Ed25519 公钥（32 字节），由上位机签名工具生成
// TODO: 替换为实际公钥
#define configED25519_PUBKEY  { \
    0x92, 0x57, 0xE1, 0xA2, \
    0xA2, 0x7D, 0xA9, 0xF6, \
    0xAD, 0x78, 0x9D, 0xC0, \
    0xB6, 0x6A, 0x75, 0x27, \
    0xE9, 0xCD, 0x4B, 0x64, \
    0xF4, 0x01, 0xF2, 0x5E, \
    0xDB, 0x3E, 0x23, 0xE6, \
    0x96, 0x93, 0x2C, 0x6B \
}

// Footer 魔术字
#define configFOOTER_MAGIC          0xAA55F00D

#endif
