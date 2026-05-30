#ifndef __CONFIG_BOOTLOADER_H__
#define __CONFIG_BOOTLOADER_H__

#define Yes                1
#define No                 0

// ============================================================
// A/B 双分区 OTA 布局
// ============================================================
// BootLoader:   Sector 0-1   (32KB, 0x08000000)
// OTA Params:   Sector 2     (16KB, 0x08008000)
// Partition A:  Sector 3-4   (80KB, 0x0800C000)  默认 / 出厂分区
// Partition B:  Sector 5     (128KB,0x08020000)  仅前 80KB 有效
// Reserved:     Sector 6-7   (256KB,0x08040000)  预留
// ============================================================

// --- Partition A ---
#define configPART_A_ADDRESS       0x0800C000
#define configPART_A_SECTOR        3
#define configPART_A_SECTOR_NUM    2     // Sectors 3+4 (16KB+64KB)

// --- Partition B ---
#define configPART_B_ADDRESS       0x08020000
#define configPART_B_SECTOR        5
#define configPART_B_SECTOR_NUM    1     // Sector 5 (128KB, 仅前 80KB 用于 App)

// --- App 约束 ---
#define configAPP_MAX_SIZE         (80 * 1024)

// --- OTA 参数区 ---
#define configPARAM_ADDRESS        0x08008000
#define configPARAM_SECTOR         2
#define configPARAM_SECTOR_NUMBER  1
#define configOTA_VALID_MAGIC      0x55AA55AA


#define configUART         Yes
#if(configUART)
    #define configRX_BUF_SIZE        2048
#endif

#define configUSE_CUSTOM_FLASH       No

#endif
