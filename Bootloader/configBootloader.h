#ifndef __CONFIG_BOOTLOADER_H__
#define __CONFIG_BOOTLOADER_H__

#define Yes                1
#define No                 0

// App起始地址
#define configAPP_ADDRESS   0x0800C000
#define configAPP_SECTOR    3
#define configAPP_SECTOR_NUMBER 3

#define configPARAM_ADDRESS      0x08008000
#define configPARAM_SECTOR 2
#define configPARAM_SECTOR_NUMBER 1
#define configOTA_VALID_MAGIC     0x55AA55AA


#define configUART         Yes
#if(configUART)
    #define configRX_BUF_SIZE        2048
#endif

#define configUSE_CUSTOM_FLASH       No

#endif
