/* ============================================================
 * LoRa 模块驱动实现 (ATK-MWCC68D)
 *
 * ATK-MWCC68D 是一款基于 UART 接口的 LoRa 无线串口模块。
 * - MD0 引脚: HIGH = AT 指令配置模式, LOW = 数据透传模式
 * - AUX 引脚: HIGH = 模块忙, LOW = 模块空闲
 * - AT 指令以 "\r\n" 结尾，响应以 "OK" 或 "ERROR" 结尾
 *
 * 注意：本驱动不直接操作任何硬件寄存器，所有 UART I/O 与
 *       GPIO 操作均通过 LoRa_Callback_t 回调注入，由调用者
 *       在 bootLoRa_Init() 时提供实现。
 * ============================================================ */

#include "lora.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 宏定义
 * ============================================================ */

/* 默认模块配置参数 */
#define configLORA_DEFAULT_ADDR          0x0000
#define configLORA_DEFAULT_NET_ID        0
#define configLORA_DEFAULT_CHANNEL       23
#define configLORA_DEFAULT_AIR_RATE      LORA_AIR_RATE_19_2Kbps
#define configLORA_DEFAULT_TX_POWER      LORA_TX_POWER_20dBm
#define configLORA_DEFAULT_WORK_MODE     LORA_MODE_GENERAL
#define configLORA_DEFAULT_BAUD_RATE     LORA_BAUD_115200
#define configLORA_DEFAULT_PARITY        LORA_PARITY_NONE
#define configLORA_DEFAULT_PACKET_SIZE   LORA_PACKET_240
#define configLORA_DEFAULT_DATA_KEY      0xF1F2F3F4
#define configLORA_DEFAULT_LBT_ENABLE    0
#define configLORA_DEFAULT_ECHO_ENABLE   1

/* ============================================================
 * 静态变量
 * ============================================================ */

/* 回调函数结构体副本，由 bootLoRa_Init() 注入 */
static LoRa_Callback_t s_callbacks;

/* 当前模块配置（初始化为默认值） */
static LoRa_Config_t s_current_config =
{
    .addr        = configLORA_DEFAULT_ADDR,
    .net_id      = configLORA_DEFAULT_NET_ID,
    .channel     = configLORA_DEFAULT_CHANNEL,
    .air_rate    = configLORA_DEFAULT_AIR_RATE,
    .tx_power    = configLORA_DEFAULT_TX_POWER,
    .work_mode   = configLORA_DEFAULT_WORK_MODE,
    .baud_rate   = configLORA_DEFAULT_BAUD_RATE,
    .parity      = configLORA_DEFAULT_PARITY,
    .packet_size = configLORA_DEFAULT_PACKET_SIZE,
    .data_key    = configLORA_DEFAULT_DATA_KEY,
    .lbt_enable  = configLORA_DEFAULT_LBT_ENABLE,
    .echo_enable = configLORA_DEFAULT_ECHO_ENABLE,
};

/* 模块初始化状态标志 */
static uint8_t s_is_initialized = 0;

/* ============================================================
 * 静态辅助函数前向声明
 * ============================================================ */

static int8_t  WaitForAuxLow_(uint32_t timeout_ms);
static int8_t  ReadLine_(char *buf, uint16_t max_len, uint32_t timeout_ms);
static int8_t  LineContainsOKOrError_(const char *line);

/* ---- 模块内部 AT 指令操作 ---- */
static int8_t  bootLoRa_SendCmd_(const char *cmd, uint16_t timeout_ms);
static int8_t  bootLoRa_SetConfig_(LoRa_Config_t *cfg);
static void    bootLoRa_EnterATMode_(void);

/* ============================================================
 * 静态辅助函数实现
 * ============================================================ */

/**
 * @brief  阻塞等待 AUX 引脚拉低（模块空闲）
 * @param  timeout_ms  超时时间 (ms)
 * @retval 0: 成功; -1: 超时
 */
static int8_t WaitForAuxLow_(uint32_t timeout_ms)
{
    uint32_t start = s_callbacks.get_tick_cb();

    while (s_callbacks.read_aux_cb())
    {
        if ((s_callbacks.get_tick_cb() - start) >= timeout_ms)
        {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief  从 UART 读取一行（以 '\n' 结尾）
 * @param  buf         存放行内容的缓冲区
 * @param  max_len     缓冲区最大长度（含 '\0'）
 * @param  timeout_ms  行读取超时 (ms)
 * @retval 0: 成功; -1: 超时; -2: 缓冲区满
 */
static int8_t ReadLine_(char *buf, uint16_t max_len, uint32_t timeout_ms)
{
    uint16_t idx = 0;
    uint8_t  byte;
    uint32_t start = s_callbacks.get_tick_cb();

    while (idx < (max_len - 1))
    {
        if (s_callbacks.read_byte_cb(&byte))
        {
            start = s_callbacks.get_tick_cb();
            buf[idx++] = (char)byte;

            if (byte == '\n')
            {
                break;
            }
        }

        if ((s_callbacks.get_tick_cb() - start) >= timeout_ms)
        {
            buf[idx] = '\0';
            return (idx > 0) ? 0 : -1;
        }
    }

    buf[idx] = '\0';
    return (idx < (max_len - 1)) ? 0 : -2;
}

/**
 * @brief  检查一行中是否包含 "OK" 或 "ERROR"
 * @param  line  行字符串
 * @retval 0: 包含 OK; -1: 包含 ERROR; -2: 都不包含
 */
static int8_t LineContainsOKOrError_(const char *line)
{
    if (line == NULL)
    {
        return -2;
    }

    if (strstr(line, "OK") != NULL)
    {
        return 0;
    }
    if (strstr(line, "ERROR") != NULL)
    {
        return -1;
    }
    return -2;
}

/**
 * @brief  进入 AT 指令配置模式（模块内部使用）
 */
static void bootLoRa_EnterATMode_(void)
{
    uint32_t start;

    s_callbacks.set_md0_cb(1);
    WaitForAuxLow_(configLORA_AT_TIMEOUT_MS);

    /* AUX 拉低仅表示 RF 硬件空闲，AT 指令解析器需要额外初始化时间 */
    start = s_callbacks.get_tick_cb();
    while ((s_callbacks.get_tick_cb() - start) < configLORA_AT_PARSER_WARMUP_MS)
    {
    }
}

/**
 * @brief  发送 AT 指令并等待 OK/ERROR 响应（模块内部使用）
 *
 * 处理流程：
 *  1. 等待 AUX 拉低（模块空闲）
 *  2. 发送 "cmd\r\n"
 *  3. 循环逐行读取，寻找包含 OK 或 ERROR 的行，其余行丢弃
 *
 * @param  cmd         AT 指令字符串（不含换行符）
 * @param  timeout_ms  超时时间 (ms)
 * @retval 0: 收到 OK; -1: 收到 ERROR; -2: 超时
 */
static int8_t bootLoRa_SendCmd_(const char *cmd, uint16_t timeout_ms)
{
    if (cmd == NULL || s_is_initialized == 0)
    {
        return -1;
    }

    /* 1. 等待模块空闲 */
    if (WaitForAuxLow_(timeout_ms) != 0)
    {
        return -2;
    }

    /* 2. 发送指令 + "\r\n" */
    while (*cmd)
    {
        s_callbacks.send_byte_cb((uint8_t)*cmd++);
    }
    s_callbacks.send_byte_cb('\r');
    s_callbacks.send_byte_cb('\n');

    /* 3. 设定总超时截止时间，逐行读取响应 */
    uint32_t deadline = s_callbacks.get_tick_cb() + (uint32_t)timeout_ms;

    for (;;)
    {
        uint32_t now = s_callbacks.get_tick_cb();
        if (now >= deadline)
        {
            return -2;
        }

        char line_buf[configLORA_AT_RESP_BUF_SIZE] = {0};
        int8_t ret = ReadLine_(line_buf, sizeof(line_buf), timeout_ms);
        if (ret < 0)
        {
            return -2;
        }

        /* 跳过空行 */
        if (line_buf[0] == '\r' && line_buf[1] == '\n' && line_buf[2] == '\0')
        {
            continue;
        }

        /* 检查此行是否包含 OK 或 ERROR */
        ret = LineContainsOKOrError_(line_buf);
        if (ret == 0 || ret == -1)
        {
            return ret;
        }

        /* 其他行丢弃，继续读取 */
    }
}

/**
 * @brief  发送 AT 指令批量配置模块参数（模块内部使用）
 */
static int8_t bootLoRa_SetConfig_(LoRa_Config_t *cfg)
{
    char cmd_buf[64];
    int8_t ret;

    if (cfg == NULL || s_is_initialized == 0)
    {
        return -1;
    }

    /* 1. AT — 基本连通性测试 */
    ret = bootLoRa_SendCmd_("AT", configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 2. AT+UART=<rate>,<parity> — 串口配置 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+UART=%d,%d",
             cfg->baud_rate, cfg->parity);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 3. AT+ADDR=<h>,<l> — 设备地址 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+ADDR=%02X,%02X",
             (cfg->addr >> 8) & 0xFF, cfg->addr & 0xFF);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 4. AT+NETID=<id> — 网络地址 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+NETID=%d", cfg->net_id);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 5. AT+WLRATE=<ch>,<rate> — 信道与空中速率 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+WLRATE=%d,%d",
             cfg->channel, cfg->air_rate);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 6. AT+TPOWER=<power> — 发射功率 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+TPOWER=%d", cfg->tx_power);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 7. AT+CWMODE=<mode> — 工作模式 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+CWMODE=%d", cfg->work_mode);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 8. AT+PACKSIZE=<size> — 数据包大小 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+PACKSIZE=%d", cfg->packet_size);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 9. AT+TMODE=0 — 透明传输模式 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+TMODE=0");
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 10. AT+DATAKEY=<key> — 数据加密密钥 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+DATAKEY=%08lX",
             (unsigned long)cfg->data_key);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    /* 11. AT+LBT=<enable> — 信道检测 */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+LBT=%d", cfg->lbt_enable);
    ret = bootLoRa_SendCmd_(cmd_buf, configLORA_AT_TIMEOUT_MS);
    if (ret != 0)
    {
        return -1;
    }

    s_current_config = *cfg;

    return 0;
}

/* ============================================================
 * 公共 API 函数实现
 * ============================================================ */

/**
 * @brief  初始化 LoRa 模块并完成 AT 配置序列
 */
int8_t bootLoRa_Init(LoRa_Callback_t *cb)
{
    if (cb == NULL)
    {
        return -1;
    }

    if (cb->read_byte_cb == NULL
        || cb->send_byte_cb == NULL
        || cb->get_tick_cb == NULL
        || cb->set_md0_cb == NULL
        || cb->read_aux_cb == NULL)
    {
        return -1;
    }

    s_callbacks = *cb;

    s_callbacks.set_md0_cb(0);

    if (WaitForAuxLow_(configLORA_INIT_TIMEOUT_MS) != 0)
    {
        return -1;
    }

    s_is_initialized = 1;

    bootLoRa_EnterATMode_();

    int8_t ret = bootLoRa_SetConfig_(&s_current_config);
    if (ret != 0)
    {
        bootLoRa_ExitATMode();
        return -2;
    }

    bootLoRa_ExitATMode();

    return 0;
}

/**
 * @brief  退出 AT 指令配置模式，进入数据透传模式
 */
void bootLoRa_ExitATMode(void)
{
    WaitForAuxLow_(configLORA_AT_TIMEOUT_MS);
    s_callbacks.set_md0_cb(0);
}

/**
 * @brief  查询模块忙状态
 */
uint8_t bootLoRa_IsBusy(void)
{
    return s_callbacks.read_aux_cb();
}
