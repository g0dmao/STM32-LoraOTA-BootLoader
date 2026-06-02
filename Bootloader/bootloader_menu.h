#ifndef __BOOTLOADER_MENU_H__
#define __BOOTLOADER_MENU_H__

#include <stdint.h>

/**
 * @brief  菜单动作枚举（供 main.c 根据返回值决定 goto 目标）
 */
typedef enum BootMenu_Action
{
    BOOTMENU_ACTION_NONE,         /* 无操作，继续等待                */
    BOOTMENU_ACTION_JUMP_APP,     /* 跳转 APP                       */
    BOOTMENU_ACTION_ENTER_OTA,    /* 进入 OTA 下载模式              */
    BOOTMENU_ACTION_ENTER_MENU,   /* 进入交互菜单                   */
} BootMenu_Action_t;

/**
 * @brief  打印启动横幅
 */
void BootMenu_PrintBanner(void);

/**
 * @brief  倒计时轮询：检测 PA0、回车键、超时
 * @param  elapsed_ms  从上电到当前的毫秒数
 * @return 下一步动作
 */
BootMenu_Action_t BootMenu_Poll(uint32_t elapsed_ms);

/**
 * @brief  进入交互菜单模式，阻塞等待用户输入
 * @return 下一步动作（JUMP_APP 或 ENTER_OTA）
 */
BootMenu_Action_t BootMenu_Interactive(void);

#endif
