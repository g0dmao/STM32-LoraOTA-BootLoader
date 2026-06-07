#ifndef __LORA_H__
#define __LORA_H__

#include <stdint.h>

/* ============================================================
 * 模块工作模式（对应 AT+CWMODE 参数）
 * ============================================================ */
#define LORA_MODE_GENERAL          0  /* 一般模式（默认）：透明/定向数据传输 */
#define LORA_MODE_WAKEUP           1  /* 唤醒模式：发送前自动添加唤醒码 */
#define LORA_MODE_POWER_SAVE       2  /* 省电模式：串口接收关闭，无线空中唤醒监听 */
#define LORA_MODE_SIGNAL_STRENGTH  3  /* 信号强度模式：输出 SNR 和 RSSI 信息 */
#define LORA_MODE_SLEEP            4  /* 睡眠模式：深度睡眠，MD0 上升沿唤醒 */
#define LORA_MODE_RELAY            5  /* 中继模式：在两个网络地址间双向转发 */

/* ============================================================
 * 发射功率（对应 AT+TPOWER 参数）
 * ============================================================ */
#define LORA_TX_POWER_9dBm   0
#define LORA_TX_POWER_11dBm  1
#define LORA_TX_POWER_14dBm  2
#define LORA_TX_POWER_17dBm  3
#define LORA_TX_POWER_20dBm  4  /* 默认 */
#define LORA_TX_POWER_22dBm  5

/* ============================================================
 * 空中速率（对应 AT+WLRATE 速率参数）
 * ============================================================ */
#define LORA_AIR_RATE_1_2Kbps   0
#define LORA_AIR_RATE_2_4Kbps   2
#define LORA_AIR_RATE_4_8Kbps   3
#define LORA_AIR_RATE_9_6Kbps   4
#define LORA_AIR_RATE_19_2Kbps  5  /* 默认 */
#define LORA_AIR_RATE_38_4Kbps  6
#define LORA_AIR_RATE_62_5Kbps  7

/* ============================================================
 * 串口波特率（对应 AT+UART 波特率参数）
 * ============================================================ */
#define LORA_BAUD_1200    0
#define LORA_BAUD_2400    1
#define LORA_BAUD_4800    2
#define LORA_BAUD_9600    3
#define LORA_BAUD_19200   4
#define LORA_BAUD_38400   5
#define LORA_BAUD_57600   6
#define LORA_BAUD_115200  7  /* 默认 */

/* ============================================================
 * 串口校验位（对应 AT+UART 校验参数）
 * ============================================================ */
#define LORA_PARITY_NONE  0  /* 无校验（默认） */
#define LORA_PARITY_EVEN  1  /* 偶校验 */
#define LORA_PARITY_ODD   2  /* 奇校验 */

/* ============================================================
 * 数据包大小（对应 AT+PACKSIZE 参数）
 * ============================================================ */
#define LORA_PACKET_32   0
#define LORA_PACKET_64   1
#define LORA_PACKET_128  2
#define LORA_PACKET_240  3  /* 默认 */

/* ============================================================
 * 休眠时间（对应 AT+WLTIME 参数）
 * ============================================================ */
#define LORA_SLEEP_TIME_1S  0  /* 默认 */
#define LORA_SLEEP_TIME_2S  1

/* ============================================================
 * 超时与缓冲区配置
 * ============================================================ */

/* AT 指令响应超时 (ms) */
#define configLORA_AT_TIMEOUT_MS    5000

/* 模块上电初始化超时 (ms)，等待 AUX 拉低表示就绪 */
#define configLORA_INIT_TIMEOUT_MS  3000

/* 进入 AT 模式后 AT 指令解析器预热时间 (ms)
   AUX 拉低仅表示 RF 硬件空闲，解析器固件需要额外初始化时间 */
#define configLORA_AT_PARSER_WARMUP_MS  800

/* AT 响应缓冲区大小 */
#define configLORA_AT_RESP_BUF_SIZE 128

/* ============================================================
 * 模块配置结构体
 * ============================================================ */
typedef struct LoRa_Config
{
    uint16_t addr;              /* 设备地址 (0~0xFFFF)，默认 0 */
    uint8_t  net_id;            /* 网络地址 (0~255)，默认 0 */
    uint8_t  channel;           /* 信道 (0~83)，频率 = 410MHz + channel，默认 23 (433MHz) */
    uint8_t  air_rate;          /* 空中速率，参考 LORA_AIR_RATE_*，默认 LORA_AIR_RATE_19_2Kbps */
    uint8_t  tx_power;          /* 发射功率，参考 LORA_TX_POWER_*，默认 LORA_TX_POWER_20dBm */
    uint8_t  work_mode;         /* 工作模式，参考 LORA_MODE_*，默认 LORA_MODE_GENERAL */
    uint8_t  baud_rate;         /* 串口波特率，参考 LORA_BAUD_*，默认 LORA_BAUD_115200 */
    uint8_t  parity;            /* 串口校验位，参考 LORA_PARITY_*，默认 LORA_PARITY_NONE */
    uint8_t  packet_size;       /* 数据包大小，参考 LORA_PACKET_*，默认 LORA_PACKET_240 */
    uint32_t data_key;          /* 数据加密密钥 (4 字节)，默认 0xF1F2F3F4 */
    uint8_t  lbt_enable;        /* 信道检测使能：0=关闭(默认), 1=打开 */
    uint8_t  echo_enable;       /* AT 指令回显：0=关闭, 1=开启(默认) */
} LoRa_Config_t;

/* ============================================================
 * 硬件操作回调函数指针结构体
 *
 * LoRa 驱动不直接绑定任何 UART 或 GPIO 外设，所有硬件操作
 * 均通过此结构体注入。调用者在 bootLoRa_Init() 时传入实现。
 * ============================================================ */
typedef struct LoRa_Callback
{
    /* ---- UART I/O ---- */

    /**
     * @brief  从 UART 读取一个字节
     * @param  pData  存放读取数据的指针
     * @retval 1: 成功读取; 0: 无数据可读
     */
    uint8_t (*read_byte_cb)(uint8_t *pData);

    /**
     * @brief  向 UART 阻塞发送一个字节
     * @param  data  要发送的字节
     */
    void (*send_byte_cb)(uint8_t data);

    /**
     * @brief  获取系统毫秒级 tick
     * @retval 当前系统 tick 值 (ms)
     */
    uint32_t (*get_tick_cb)(void);

    /* ---- MD0 / AUX GPIO 操作 ---- */

    /**
     * @brief  设置 MD0 引脚电平
     * @param  level  1 = 高电平（AT 指令模式）; 0 = 低电平（数据透传模式）
     */
    void (*set_md0_cb)(uint8_t level);

    /**
     * @brief  读取 AUX 引脚电平
     * @retval 1: 高电平（模块忙）; 0: 低电平（模块空闲）
     */
    uint8_t (*read_aux_cb)(void);
} LoRa_Callback_t;

/* ============================================================
 * 公共 API 函数声明
 * ============================================================ */

/**
 * @brief  初始化 LoRa 模块并完成 AT 配置序列
 * @note   调用前需确保：
 *         1. 对应 UART 外设、MD0 和 AUX 的 GPIO 已由 CubeMX 初始化
 *         2. 模块已完成上电
 * @param  cb   硬件操作回调函数指针结构体（含 UART I/O 与 GPIO 操作）
 * @retval 0: 成功; -1: 参数无效/模块无响应; -2: AT 配置失败
 */
int8_t bootLoRa_Init(LoRa_Callback_t *cb);

/**
 * @brief  退出 AT 指令配置模式，进入数据透传模式
 * @note   将 MD0 引脚拉低，确保模块可透传串口数据
 */
void bootLoRa_ExitATMode(void);

/**
 * @brief  查询模块忙状态
 * @retval 1: 模块忙（AUX=HIGH）; 0: 模块空闲（AUX=LOW）
 */
uint8_t bootLoRa_IsBusy(void);

#endif /* __LORA_H__ */
