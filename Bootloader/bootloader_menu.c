#include "bootloader_menu.h"

#include <stdio.h>
#include "main.h"
#include "configBootloader.h"
#include "uart_dma_ring.h"
#include "ota.h"
#include "flasher.h"

/* ================================================================
 * 静态辅助函数前向声明
 * ================================================================ */

static void PrintSeparator_(char ch, int len);
static void PrintOTAParams_(void);
static void PrintAuthor_(void);
static void PrintMenuPrompt_(void);

/* ================================================================
 * 公共函数实现
 * ================================================================ */

/**
 * @brief  打印启动横幅
 */
void BootMenu_PrintBanner(void)
{
    PrintSeparator_('=', 41);

    printf("      *:..:D GoDm BootLoader :D:..:*\r\n");
    printf("       STM32F411CEU6  OTA System\r\n");

    PrintSeparator_('=', 41);
    printf("\r\n");
}

/**
 * @brief  倒计时轮询：检测 PA0、回车键、超时
 * @param  elapsed_ms  从上电到当前的毫秒数
 * @return 下一步动作
 */
BootMenu_Action_t BootMenu_Poll(uint32_t elapsed_ms)
{
    static int s_last_remaining = -1;

    /* 1. 检测 PA0 是否拉低 → 进入 OTA */
    if (!(LL_GPIO_ReadInputPort(GPIOA) & BOOT_Pin))
    {
        printf("\r\n[PA0] Entering OTA download mode...\r\n");
        return BOOTMENU_ACTION_ENTER_OTA;
    }

    /* 2. 检测串口是否收到回车键 → 进入交互菜单 */
    uint8_t ch;
    if (bootUART_ReadByte(&ch) == 1)
    {
        if (ch == '\r' || ch == '\n')
        {
            printf("\r\n");
            return BOOTMENU_ACTION_ENTER_MENU;
        }
    }

    /* 3. 每秒刷新一次倒计时 */
    int remaining = (configMS_TO_JUMP - elapsed_ms + 999) / 1000;
    if (remaining < 0)
    {
        remaining = 0;
    }

    if (remaining != s_last_remaining)
    {
        s_last_remaining = remaining;
        if (remaining > 0)
        {
            printf("\r=> Auto-booting App in %ds ... (PA0=OTA | Enter=Menu)\r\n",
                   remaining);
        }
    }

    /* 4. 超时 → 跳转 APP */
    if (elapsed_ms >= configMS_TO_JUMP)
    {
        printf("\r\n");
        return BOOTMENU_ACTION_JUMP_APP;
    }

    return BOOTMENU_ACTION_NONE;
}

/**
 * @brief  进入交互菜单模式，阻塞等待用户输入
 * @return 下一步动作（JUMP_APP 或 ENTER_OTA）
 */
BootMenu_Action_t BootMenu_Interactive(void)
{
    PrintMenuPrompt_();

    for (;;)
    {
        uint8_t ch;
        if (bootUART_ReadByte(&ch) == 1)
        {
            switch (ch)
            {
                case '1':
                    printf("\r\nJumping to APP...\r\n\r\n");
                    return BOOTMENU_ACTION_JUMP_APP;

                case '2':
                    printf("\r\nEntering OTA download mode...\r\n\r\n");
                    return BOOTMENU_ACTION_ENTER_OTA;

                case '3':
                    printf("\r\n");
                    PrintOTAParams_();
                    //PrintMenuPrompt_();
                    printf("\r\nWaiting input-_-......\r\n\r\n");
                    break;

                case '4':
                    printf("\r\n");
                    PrintAuthor_();
                    //PrintMenuPrompt_();
                    printf("\r\nWaiting input-_-......\r\n\r\n");
                    break;

                case '\r':
                case '\n':
                    /* 忽略多余的回车 */
                    break;

                default:
                    printf("\r\n[!] Invalid option: '%c'\r\n", ch);
                    printf("\r\nWaiting input-_-......\r\n\r\n");
                    break;
            }
        }
    }
}

/* ================================================================
 * 静态辅助函数实现
 * ================================================================ */

/**
 * @brief  打印一条重复字符组成的分隔线
 * @param  ch   重复字符
 * @param  len  字符个数
 */
static void PrintSeparator_(char ch, int len)
{
    for (int i = 0; i < len; i++)
    {
        printf("%c", ch);
    }
    printf("\r\n");
}

/**
 * @brief  打印 OTA 参数区数据
 */
static void PrintOTAParams_(void)
{
    OTA_Context_t ctx;
    ctx.read_cb  = bootFlasher_ReadData;
    ctx.write_cb = NULL;
    ctx.erase_cb = NULL;

    OTA_Param_t param;
    if (bootOTA_ReadParamOTA(&ctx, &param) != 0)
    {
        printf("[!] Failed to read OTA parameters!\r\n");
        return;
    }

    PrintSeparator_('-', 40);
    printf("   OTA Parameters  (o..o)\r\n");
    printf("  -----------------------------------\r\n");
    printf("  Magic Flag:       0x%08lX", (unsigned long)param.magic_flag);
    if (param.magic_flag != configOTA_VALID_MAGIC)
    {
        printf(" (INVALID!)\r\n");
    }
    else
    {
        printf(" (VALID)\r\n");
    }
    printf("  App Size:         %lu bytes\r\n", param.app_size);
    printf("  App CRC:          0x%04X\r\n", (uint16_t)param.app_crc);
    printf("  Active Partition: %c (0x%08lX)\r\n",
           (param.active_partition == 0) ? 'A' : 'B',
           (unsigned long)((param.active_partition == 0) ? configPART_A_ADDRESS
                                                          : configPART_B_ADDRESS));
    printf("  Current Version:  %lu\r\n", param.current_version);
    PrintSeparator_('-', 40);
}

/**
 * @brief  打印作者 / 项目信息
 */
static void PrintAuthor_(void)
{
    PrintSeparator_('-', 40);
    printf("   About (.. )\r\n");
    printf("  -----------------------------------\r\n");
    printf("  Project:  STM32F411 BootLoader\r\n");
    printf("  MCU:      STM32F411CEU6\r\n");
    printf("  Author:   GoDm\r\n");
    printf("  Build:    %s %s\r\n", __DATE__, __TIME__);
    PrintSeparator_('-', 40);
}

/**
 * @brief  打印交互菜单选项
 */
static void PrintMenuPrompt_(void)
{
    PrintSeparator_('@', 40);
    printf("   BootLoader Menu  (~o~)\r\n");
    printf("  -----------------------------------\r\n");
    printf("   1. Jump to Application\r\n");
    printf("   2. Enter OTA Download Mode\r\n");
    printf("   3. Display OTA Parameters\r\n");
    printf("   4. About / Author Info\r\n");
    PrintSeparator_('@', 40);
    printf("Please select [1-4]: ");
}
