#ifndef __BOOTLOADER_MENU_H__
#define __BOOTLOADER_MENU_H__

#include <stdint.h>

/*
    场景                              行为
上电 + 无操作           横幅 → 3秒倒计时 → 校验 → 跳转 APP
上电 + 按住 PA0         立即进入有线 OTA 下载模式
倒计时期间按回车        进入交互菜单，循环等待输入
菜单按 1                校验并跳转 APP
菜单按 2                进入有线 Ymodem OTA
菜单按 3                进入 LoRa 无线 Ymodem OTA
菜单按 4                显示 OTA 参数区数据，然后重新显示菜单
菜单按 5                显示作者/项目信息，然后重新显示菜单
菜单按其他键            提示无效输入，重新显示菜单
LoRa AUX 拉高           自动进入 LoRa 无线 OTA
*/

/**
 * @brief  菜单动作枚举（供 main.c 根据返回值决定 goto 目标）
 */
typedef enum BootMenu_Action
{
    BOOTMENU_ACTION_NONE,               /* 无操作，继续等待                */
    BOOTMENU_ACTION_JUMP_APP,           /* 跳转 APP                       */
    BOOTMENU_ACTION_ENTER_OTA,          /* 进入有线 Ymodem OTA            */
    BOOTMENU_ACTION_ENTER_LORA_OTA,     /* 进入 LoRa 无线 Ymodem OTA      */
    BOOTMENU_ACTION_ENTER_MENU,         /* 进入交互菜单                   */
    BOOTMENU_ACTION_PRINT_OTA_PARAMS,   /* 打印 OTA 参数（由 main.c 处理） */
} BootMenu_Action_t;

/**
 * @brief  菜单上下文：通过依赖注入解耦硬件依赖
 * @note   由 main.c 在 BootloaderInit() 中填充，菜单服务层不依赖任何具体硬件模块
 */
typedef struct BootMenu_Context
{
    uint8_t (*read_byte_cb)(uint8_t *p_data);   /**< 读取一个字节（1=成功, 0=空） */
    uint8_t (*read_ota_pin_cb)(void);           /**< 检测 OTA 触发引脚（1=有效）  */
} BootMenu_Context_t;

/**
 * @brief  打印启动横幅
 */
void BootMenu_PrintBanner(void);

/**
 * @brief  倒计时轮询：检测 OTA 引脚、回车键、超时
 * @param  ctx         菜单上下文（含回调）
 * @param  elapsed_ms  从上电到当前的毫秒数
 * @return 下一步动作
 */
BootMenu_Action_t BootMenu_Poll(BootMenu_Context_t *ctx, uint32_t elapsed_ms);

/**
 * @brief  进入交互菜单模式，阻塞等待用户输入
 * @param  ctx  菜单上下文（含回调）
 * @return 下一步动作
 */
BootMenu_Action_t BootMenu_Interactive(BootMenu_Context_t *ctx);

#endif
