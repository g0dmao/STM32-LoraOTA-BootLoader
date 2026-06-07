#include "bootloader_menu.h"

#include <stdio.h>
#include "configBootloader.h"

/* ================================================================
 * 静态辅助函数前向声明
 * ================================================================ */

static void PrintSeparator_(char ch, int len);
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
 * @brief  倒计时轮询：检测 OTA 引脚、回车键、超时
 * @param  ctx         菜单上下文（含回调）
 * @param  elapsed_ms  从上电到当前的毫秒数
 * @return 下一步动作
 */
BootMenu_Action_t BootMenu_Poll(BootMenu_Context_t *ctx, uint32_t elapsed_ms)
{
    static int s_last_remaining = -1;

    /* 1. 检测 OTA 引脚是否拉低 → 进入 OTA */
    if (ctx->read_ota_pin_cb())
    {
        printf("\r\n[PA0] Entering OTA download mode...\r\n");
        return BOOTMENU_ACTION_ENTER_OTA;
    }

    /* 2. 检测串口是否收到回车键 → 进入交互菜单 */
    uint8_t ch;
    if (ctx->read_byte_cb(&ch) == 1)
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
 * @param  ctx  菜单上下文（含回调）
 * @return 下一步动作
 */
BootMenu_Action_t BootMenu_Interactive(BootMenu_Context_t *ctx)
{
    PrintMenuPrompt_();

    for (;;)
    {
        uint8_t ch;
        if (ctx->read_byte_cb(&ch) == 1)
        {
            switch (ch)
            {
                case '1':
                    printf("\r\nJumping to APP...\r\n\r\n");
                    return BOOTMENU_ACTION_JUMP_APP;

                case '2':
                    printf("\r\nEntering wired Ymodem OTA...\r\n\r\n");
                    return BOOTMENU_ACTION_ENTER_OTA;

                case '3':
                    printf("\r\nEntering LoRa wireless OTA...\r\n\r\n");
                    return BOOTMENU_ACTION_ENTER_LORA_OTA;

                case '4':
                    printf("\r\n");
                    return BOOTMENU_ACTION_PRINT_OTA_PARAMS;

                case '5':
                    printf("\r\n");
                    PrintAuthor_();
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
    printf("   2. Wired Ymodem OTA\r\n");
    printf("   3. LoRa Wireless OTA\r\n");
    printf("   4. Display OTA Parameters\r\n");
    printf("   5. About / Author Info\r\n");
    PrintSeparator_('@', 40);
    printf("Please select [1-5]: ");
}
