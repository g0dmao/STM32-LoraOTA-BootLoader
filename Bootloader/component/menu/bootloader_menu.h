#ifndef __BOOTLOADER_MENU_H__
#define __BOOTLOADER_MENU_H__

#include <stdint.h>

/*
    场景	                            行为
上电 + 无操作	         横幅 → 3秒倒计时 → 校验 → 跳转 APP
上电 + 按住 PA0	        立即进入有线 OTA 下载模式
倒计时期间按回车	     进入交互菜单，循环等待输入
菜单按 1	           校验并跳转 APP
菜单按 2	           进入有线 Ymodem OTA
菜单按 3	           进入 LoRa 无线 Ymodem OTA
菜单按 4	           显示 OTA 参数区数据，然后重新显示菜单
菜单按 5	           显示作者/项目信息，然后重新显示菜单
菜单按其他键	        提示无效输入，重新显示菜单
LoRa AUX 拉高         自动进入 LoRa 无线 OTA
*/

/**
 * @brief  菜单动作枚举（供 main.c 根据返回值决定 goto 目标）
 */
typedef enum BootMenu_Action
{
    BOOTMENU_ACTION_NONE,             /* 无操作，继续等待                */
    BOOTMENU_ACTION_JUMP_APP,         /* 跳转 APP                       */
    BOOTMENU_ACTION_ENTER_OTA,        /* 进入有线 Ymodem OTA            */
    BOOTMENU_ACTION_ENTER_LORA_OTA,   /* 进入 LoRa 无线 Ymodem OTA      */
    BOOTMENU_ACTION_ENTER_MENU,       /* 进入交互菜单                   */
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
