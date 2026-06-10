# 面向 STM32 平台的 OTA 安全固件升级系统 BootLoader 技术实现文档

> **项目名称**: STM32-LoraOTA-BootLoader

> **目标平台**: STM32F411CEU6 (Cortex-M4, 512KB Flash, 128KB RAM)

> **构建系统**: CMake 3.22+, ARM GCC (arm-none-eabi)

> **作者**: [GoDm@](https://github.com/g0dmao)

**该项目使用到的开源库:**

- [janpatch | Apache 2.0 License](https://github.com/janjongboom/janpatch)
- [libhydrogen | ISC License](https://github.com/jedisct1/libhydrogen)

**想将此项目移植到其他 STM32 系列 MCU，请参见[移植指南 (PORTING.md)](PORTING.md)**

****该文档由 Deepseek-v4-pro 生成***

---

## 目录

1. [系统概述](#1-系统概述)
2. [Flash 分区布局](#2-flash-分区布局)
3. [架构设计](#3-架构设计)
4. [模块详解](#4-模块详解)
   - [4.1 启动流程 (main.c)](#41-启动流程-mainc)
   - [4.2 配置系统 (configBootloader.h)](#42-配置系统-configbootloaderh)
   - [4.3 跳转逻辑 (bootloader)](#43-跳转逻辑-bootloader)
   - [4.4 OTA 下载编排 (ota_download)](#44-ota-下载编排-ota_download)
   - [4.5 Ymodem 协议 (ymodem)](#45-ymodem-协议-ymodem)
   - [4.6 A/B 分区管理 (ota)](#46-ab-分区管理-ota)
   - [4.7 签名验证 (sign_verify)](#47-签名验证-sign_verify)
   - [4.8 差量更新 (diff_update + janpatch)](#48-差量更新-diff_update--janpatch)
   - [4.9 用户交互菜单 (bootloader_menu)](#49-用户交互菜单-bootloader_menu)
   - [4.10 LoRa 无线模块驱动 (lora)](#410-lora-无线模块驱动-lora)
   - [4.11 UART DMA 环形缓冲 (uart_dma_ring)](#411-uart-dma-环形缓冲-uart_dma_ring)
   - [4.12 Flash 驱动 (flasher)](#412-flash-驱动-flasher)
   - [4.13 密码学库 (libhydrogen)](#413-密码学库-libhydrogen)
5. [上位机工具](#5-上位机工具)
6. [关键设计决策](#6-关键设计决策)
7. [OTA 升级完整流程](#7-ota-升级完整流程)

---

## 1. 系统概述

STM32-LoraOTA-BootLoader 是一个为 STM32F411CEU6 设计的嵌入式引导加载程序，支持通过**有线串口 (UART)** 和 **无线 LoRa** 两种通道对固件进行 **OTA (Over-The-Air) 升级**。

### 核心能力

| 功能 | 说明 |
|------|------|
| **A/B 双分区** | 出厂分区 (A) + 升级分区 (B)，升级失败不影响当前运行固件 |
| **双通道升级** | 有线 Ymodem (UART) 和 无线 Ymodem-over-LoRa |
| **全量/差量更新** | 支持完整固件传输和基于 JANPatch 的二进制差量补丁 |
| **Ed25519 签名** | 基于 libhydrogen 的数字签名校验，防止恶意固件刷入 |
| **防回滚保护** | 基于单调递增版本号的固件降级拦截 |
| **CRC16 完整性校验** | 固件写入后计算 CRC，启动前二次校验 |
| **交互式菜单** | 串口控制台提供倒计时自动启动、手动菜单、OTA 参数查看等功能 |
| **GPIO 触发** | PA0 引脚拉低强制进入 OTA 模式 |

### 性能指标

- **BootLoader 体积**: ≤ 32KB (Flash Sector 0-1)
- **RAM 占用**: 极小（栈 1KB + 堆 512B + 静态缓冲区 ~16KB）
- **启动超时**: 默认 3 秒 (可配置)
- **支持的 App 固件大小**: ≤ 128KB (含 76 字节 Footer)

---

## 2. Flash 分区布局

STM32F411CEU6 拥有 512KB 片上 Flash (8 个扇区)，分区如下：

```
┌─────────────────────────────────────────────────────────────┐
│ Sector 0-1  │ BootLoader          │ 32KB  │ 0x0800_0000    │
│ Sector 2    │ OTA Parameters      │ 16KB  │ 0x0800_8000    │
│ Sector 3    │ (Free / Reserved)   │ 16KB  │ 0x0800_C000    │
│ Sector 4    │ Patch Storage       │ 64KB  │ 0x0801_0000    │
│ Sector 5    │ Partition B         │ 128KB │ 0x0802_0000    │
│ Sector 6    │ Partition A (出厂)   │ 128KB │ 0x0804_0000    │
│ Sector 7    │ (Reserved)          │ 128KB │ 0x0806_0000    │
└─────────────────────────────────────────────────────────────┘
```

### 分区说明

- **BootLoader (32KB)**: 本项目本体，上电最先执行，负责引导决策和固件升级。
- **OTA Parameters (16KB)**: 存储 `OTA_Param_t` 结构体 (16 字节)，记录当前活跃分区、固件 CRC、版本号等元数据。独占一个扇区以保证写入时不影响其他数据。
- **Partition A (128KB, 出厂分区)**: 默认固件存放区，出厂时烧录。地址较高，通常作为"稳定"分区。
- **Partition B (128KB, 升级分区)**: OTA 升级的目标分区，与 A 区形成 A/B 双备份。
- **Patch Storage (64KB)**: 差量补丁暂存区。差量升级时补丁文件先写入此处，再通过 JANPatch 合成到目标分区。
- **Free (16KB)**: 预留扩展空间（如增加 OTA 参数区的备份页）。
- **Reserved (128KB)**: 保留，未来可用于更大固件或额外功能。

### A/B 切换策略

```
出厂状态: active_partition = 0 → 运行 Partition A

OTA 升级:
  1. 新固件写入 inactive 分区 (B)
  2. 校验通过后: active_partition = 1
  3. 重启 → 运行 Partition B

再次升级:
  1. 新固件写入 inactive 分区 (A)
  2. 校验通过后: active_partition = 0
  3. 重启 → 运行 Partition A
```

---

## 3. 架构设计

项目采用 **分层架构**，自上而下分为五层：

```
┌───────────────────────────────────────────────────────┐
│                   Application Layer                    │
│                   (Core/Src/main.c)                    │
│              状态机驱动，系统总装配和调度                 │
├───────────────────────────────────────────────────────┤
│                   Component Layer                      │
│   ┌──────────────┐  ┌──────────────────────┐          │
│   │  bootloader  │  │    ota_download      │          │
│   │  (跳转 App)   │  │  (OTA 下载编排器)     │          │
│   └──────────────┘  └──────────────────────┘          │
├───────────────────────────────────────────────────────┤
│                    Service Layer                       │
│   ┌────────┐ ┌─────┐ ┌──────────┐ ┌──────────────┐   │
│   │ ymodem │ │ ota │ │sign_verify│ │ diff_update  │   │
│   │(协议栈) │ │(分区)│ │(Ed25519) │ │(差量合成)     │   │
│   └────────┘ └─────┘ └──────────┘ └──────────────┘   │
│   ┌──────────────────┐                                │
│   │ bootloader_menu  │                                │
│   │  (用户交互界面)    │                                │
│   └──────────────────┘                                │
├───────────────────────────────────────────────────────┤
│              Hardware Abstraction Layer                │
│   ┌──────────────────────┐                            │
│   │  lora (ATK-MWCC68D)  │                            │
│   │  (LoRa 模块 AT 驱动)  │                            │
│   └──────────────────────┘                            │
├───────────────────────────────────────────────────────┤
│                   Port Layer (驱动适配)                 │
│   ┌────────────┐ ┌──────────┐ ┌──────────────────┐   │
│   │ uart_dma   │ │ flasher  │ │  janpatch_port   │   │
│   │ (DMA 环缓冲)│ │(Flash 读写)│ │ (Flash 流适配器)  │   │
│   └────────────┘ └──────────┘ └──────────────────┘   │
├───────────────────────────────────────────────────────┤
│                   Library Layer                        │
│   ┌────────────────┐ ┌─────────────┐                  │
│   │  libhydrogen   │ │  janpatch   │                  │
│   │  (Ed25519 密码库)│ │ (差量补丁库) │                  │
│   └────────────────┘ └─────────────┘                  │
└───────────────────────────────────────────────────────┘
```

### 核心设计原则

1. **回调注入 (Dependency Inversion)**: 所有服务层和硬件层的 I/O 操作均通过函数指针回调完成，不直接访问寄存器。解耦了业务逻辑和硬件实现，使各模块可独立测试、可方便地移植到其他 MCU。

2. **编译时功能开关**: `configBootloader.h` 通过 `#define Yes 1 / No 0` 和 `#if` 条件编译控制所有可选功能（LoRa、签名验证、防回滚、Footer 等），未启用的代码完全不参与编译，最小化固件体积。

3. **体积优化优先**: 全部编译选项面向最小体积（`-Os`、`-ffunction-sections`、`-fdata-sections`、`-Wl,--gc-sections`），链接时丢弃标准库（libc/libm/libgcc），使用 LL 库而非 HAL 库，手动实现简单的字符串解析（避免引入 `atoi`/`strtol` 等重型库函数）。

4. **goto 状态机**: `main()` 采用 `goto` 标签 (`Check`, `Jump`, `InteractiveMenu`, `err`) 实现主流程状态机，避免深层函数调用嵌套，保持栈深度可预测且最小。

---

## 4. 模块详解

### 4.1 启动流程 (main.c)

`Core/Src/main.c` 是固件入口，包含完整的上电启动序列。

#### 启动序列

```
上电 → Reset_Handler (startup_stm32f411xe.s)
  → SystemInit()                           // FPU、时钟基础配置
  → main()
    → HAL_Init() 等效 (LL 手动)               // 使能 SYSCFG/PWR 时钟
    → NVIC_PriorityGroup_4                  // 4 位抢占优先级
    → SystemClock_Config()                  // HSE-PLL → 100MHz
    → SysTick 使能                          // 1ms 系统 tick
    → MX_GPIO_Init()                        // PA0(BOOT), PB3(MD0), PB4(AUX)
    → MX_DMA_Init()                         // DMA2 Stream2
    → MX_USART1_UART_Init()                 // 115200-8N1
    → BootloaderInit()                      // 注入所有回调函数
    → BootMenu_PrintBanner()                // 打印启动横幅
    → 主轮询循环
```

#### 时钟配置

- **时钟源**: HSE (外部高速晶振) 25MHz
- **PLL 配置**: PLLM=/12, PLLN=x96, PLLP=/2 → 系统时钟 100MHz
- **总线分频**: AHB=/1 (100MHz), APB1=/2 (50MHz), APB2=/1 (100MHz)
- **Flash 等待周期**: 3 (100MHz 需要 3 WS)
- **SysTick**: 1ms 中断，`g_sys_tick` 作为全系统时间基准

#### 回调注入 (BootloaderInit)

```c
// Ymodem 协议层的 I/O 回调
g_ym_ctx.read_byte_cb = bootUART_ReadByte;
g_ym_ctx.send_byte_cb = bootUART_SendByte;
g_ym_ctx.get_tick_cb  = GetTick;

// OTA Flash 操作回调
g_ota_ctx.read_cb   = bootFlasher_ReadData;
g_ota_ctx.write_cb  = bootFlasher_WriteByte;
g_ota_ctx.erase_cb  = bootFlasher_EraseSectors;
g_ota_ctx.unlock_cb = bootFlasher_Unlock;
g_ota_ctx.lock_cb   = bootFlasher_Lock;

// LoRa 模块回调
g_lora_cb.read_byte_cb = bootUART_ReadByte;
g_lora_cb.send_byte_cb = bootUART_SendByte;
g_lora_cb.get_tick_cb  = GetTick;
g_lora_cb.set_md0_cb   = LoRa_SetMD0;
g_lora_cb.read_aux_cb  = LoRa_ReadAUX;

// 菜单回调
g_menu_ctx.read_byte_cb    = bootUART_ReadByte;
g_menu_ctx.read_ota_pin_cb = ReadOtaPin;
```

#### 主状态机

```
            ┌─────────────┐
            │  主轮询循环   │◄──────────────┐
            └──────┬──────┘               │
                   │                      │
        ┌──────────┼──────────┐           │
        ▼          ▼          ▼           │
   [OTA Pin]   [Enter键]   [超时]         │
        │          │          │           │
        ▼          ▼          ▼           │
    OTA下载   交互菜单    → Check         │
        │          │          │           │
        ▼          │          ▼           │
      Jump         └──────→  Jump         │
        │                     │           │
        └─────────────────────┘           │

  Check: 读取OTA参数 → 验证CRC
    ├── 通过 → Jump
    └── 失败 → err (死循环)
```

### 4.2 配置系统 (configBootloader.h)

[Boatloader/configBootloader.h](Bootloader/configBootloader.h) 是项目的**唯一配置点**，所有编译时常量集中于此：

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `configPART_A_ADDRESS` | 分区 A 起始地址 | `0x08040000` |
| `configPART_B_ADDRESS` | 分区 B 起始地址 | `0x08020000` |
| `configAPP_MAX_SIZE` | App 固件最大体积 | `128KB` |
| `configPARAM_ADDRESS` | OTA 参数存储地址 | `0x08008000` |
| `configOTA_VALID_MAGIC` | OTA 参数有效魔术字 | `0x55AA55AA` |
| `configPATCH_STORAGE_ADDRESS` | 补丁暂存区地址 | `0x08010000` |
| `configPATCH_MAX_SIZE` | 补丁文件最大体积 | `64KB` |
| `configUART` | 启用有线 UART | `Yes` |
| `configRX_BUF_SIZE` | UART DMA 缓冲大小 | `2048` |
| `configLORA` | 启用 LoRa 无线模块 | `Yes` |
| `configYM_BYTE_TIMEOUT_MS` | 字节接收超时 | `500ms`(LoRa) / `100ms`(有线) |
| `configYM_PACKET_TIMEOUT_MS` | 数据包接收超时 | `5000ms`(LoRa) / `3000ms`(有线) |
| `configYM_RETRY_COUNT` | 重传次数 | `5`(LoRa) / `10`(有线) |
| `configUSE_CUSTOM_FLASH` | 使用外部 Flash | `No` |
| `configMS_TO_JUMP` | 启动倒计时 | `3000ms` |
| `configUSE_FOOTER` | 启用固件尾部签名 | `Yes` |
| `configSIG_VERIFY_ENABLE` | 强制 Ed25519 验签 | `Yes` |
| `configROLLBACK_ENABLE` | 启用防回滚 | `Yes` |
| `configED25519_PUBKEY` | Ed25519 公钥 (32 字节) | (编译时替换) |
| `configFOOTER_MAGIC` | Footer 魔术字 | `0xAA55F00D` |

**设计要点**:
- `Yes`/`No` 定义为 `1`/`0`，使 `#if` 条件编译语法清晰自然。
- 有线和无线的 Ymodem 超时参数分为两套，无线超时更长以适应空中传输延迟。

### 4.3 跳转逻辑 (bootloader)

[Bootloader/component/bootloader/bootloader.c](Bootloader/component/bootloader/bootloader.c) 负责从 BootLoader **安全地切换到 App 固件**。

#### 跳转流程

```
Bootloader_JumpToApp(app_address)
  │
  ├─ 1. 读取 App 栈顶指针 (app_address + 0)
  │     └─ 验证: 0x20000000 ≤ SP ≤ 0x20020000 (128KB RAM)
  │
  ├─ 2. 读取 App 复位向量 (app_address + 4)
  │
  ├─ 3. 关闭全局中断 (__disable_irq)
  │
  ├─ 4. 反初始化外设 (USART1, DMA2 Stream2, GPIOA)
  │
  ├─ 5. 清理 NVIC (8 组 ICER + ICPR 全部写 1 清零)
  │
  ├─ 6. 强制特权线程模式 + MSP (__set_CONTROL(0))
  │
  ├─ 7. 重映射向量表 (SCB->VTOR = app_address)
  │
  └─ 8. 裸函数跳转 (naked + noreturn)
        └─ msr msp, r0   // 设置 MSP = app_sp
        └─ bx r1          // 跳转到 app_pc
```

#### 设计要点

- **SP 合法性检查**: 验证栈指针必须在 STM32F411 的 RAM 地址空间 (`0x20000000` - `0x20020000`)，防止跳转到损坏的固件导致系统硬错误。
- **全面清理**: 跳转前关闭所有中断、清理 NVIC 挂起位、反初始化外设、重映射向量表。确保 App 获得一个干净的系统状态，不会因为 BootLoader 遗留的中断使能或外设状态而异常。
- **裸函数**: `bootJump_Execute()` 声明为 `__attribute__((naked, noreturn))`，编译器不生成栈帧代码，直接执行内联汇编进行跳转。

### 4.4 OTA 下载编排 (ota_download)

[Bootloader/component/ota_download/ota_download.c](Bootloader/component/ota_download/ota_download.c) 是 OTA 升级流程的**总编排器**，封装了完整的下载流水线。

#### 流水线步骤

```
OtaDownload_Execute()
  │
  ├─ ConnectAndErase_()
  │   ├─ bootYM_EstablishConnection()      // 发送 'C'，接收 packet 0
  │   ├─ 解析文件名，判断全量/差量
  │   └─ 擦除目标区域:
  │       ├─ 全量: 擦除非活跃分区
  │       └─ 差量: 擦除 Patch Storage (Sector 4)
  │
  └─ ReceiveAndFlash_()
      ├─ 循环接收 Ymodem 数据包
      │   └─ bootYM_AccepctOnePacket() → 写入 Flash
      │
      ├─ EOT (传输结束):
      │   ├─ 差量模式: 擦除非活跃分区 → bootDiff_ApplyPatch()
      │   ├─ 全量模式: fw_total_size = file_size
      │   ├─ bootSIG_ParseAndVerify()        // Ed25519 验签 + 防回滚
      │   ├─ CalcCRC16()                     // 固件 CRC 校验
      │   └─ bootOTA_SaveParamOTA()          // 切换活跃分区
      │
      └─ 返回 OTADL_STATUS_OK
```

#### 差量识别

文件名中包含 `_patch` 子串即视为差量补丁：

```
firmware_v2.bin         → 全量更新包
firmware_v2_patch.bin   → 差量更新包
```

#### 固件 Footer 格式

每个合法的固件镜像尾部附加 76 字节 Footer：

```
┌────────────────────────────┐
│  firmware body (N bytes)   │  ← Ed25519 签名计算覆盖此部分
├────────────────────────────┤
│  version       (4 bytes)   │  固件版本号 (uint32_t)
│  signature     (64 bytes)  │  Ed25519 签名
│  footer_magic  (4 bytes)   │  0xAA55F00D
│  footer_size   (4 bytes)   │  sizeof(FirmwareFooter_t) = 76
└────────────────────────────┘
```

### 4.5 Ymodem 协议 (ymodem)

[Bootloader/service/ymodem/ymodem.c](Bootloader/service/ymodem/ymodem.c) 实现了完整的 Ymodem 接收端协议栈 (CRC16 变体)。

#### 协议状态机

```
发送端                        接收端 (BootLoader)
  │                              │
  │  ◄────── 'C' ─────────────── │  发起传输请求
  │                              │
  │  ─── SOH 00 FF [文件名 大小] CRC ──► │  Packet 0 (128 字节)
  │                              │
  │  ◄────── ACK ─────────────── │
  │  ◄────── 'C' ─────────────── │  请求数据包
  │                              │
  │  ─── STX 01 FE [1024B data] CRC ──► │  Packet 1
  │                              │
  │  ◄────── ACK ─────────────── │
  │           ...                │
  │                              │
  │  ─── EOT ──────────────────► │
  │  ◄────── NAK ─────────────── │  (协议要求)
  │  ─── EOT ──────────────────► │
  │  ◄────── ACK ─────────────── │
  │  ◄────── 'C' ─────────────── │  请求终止
  │                              │
```

#### 核心数据结构

```c
typedef struct {
    uint8_t  packet_data[1024];  // 当前数据包缓冲区
    uint16_t packet_len;         // 实际数据长度 (128 或 1024)
    uint32_t file_size;          // 来自 packet 0 的文件总大小
    uint32_t total_receive_byte; // 累计接收字节数
    char     file_name[64];      // 文件名（来自 packet 0）

    // 回调函数指针 (依赖注入)
    uint8_t  (*read_byte_cb)(uint8_t *);
    void     (*send_byte_cb)(uint8_t);
    uint32_t (*get_tick_cb)(void);
} YM_InfoBlock_t;
```

#### 返回码体系

| 返回值 | 含义 |
|--------|------|
| `YM_RETURN_CODE_OK` (0) | 正常接收一个数据包 |
| `YM_RETURN_CODE_ERROR` (-1) | 通用错误 |
| `YM_RETURN_CODE_TIMEOUT` (-2) | 接收超时 |
| `YM_RETURN_CODE_ABORT` (-3) | 发送端主动终止 (CAN) |
| `YM_RETURN_CODE_ERROR_DATA` (-4) | CRC 校验错误或序列号不匹配 |
| `YM_RETURN_CODE_EOT` (1) | 传输结束 |

#### CRC16 实现

- 多项式: `0x1021` (CRC-16-CCITT)
- 初始值: `0x0000`
- 两用: Ymodem 数据包校验 + 固件完整性校验 (通过 `CalcCRC16()` 公开接口)

### 4.6 A/B 分区管理 (ota)

[Bootloader/service/ota/ota.c](Bootloader/service/ota/ota.c) 管理 A/B 双分区方案和 OTA 参数持久化。

#### OTA 参数结构

```c
typedef struct __attribute__((packed)) {
    uint32_t magic_flag;       // configOTA_VALID_MAGIC (0x55AA55AA)
    uint32_t app_size;         // 固件大小
    uint16_t app_crc;          // 固件 CRC16
    uint8_t  active_partition; // 0=A 分区, 1=B 分区
    uint8_t  reserved[3];      // 保留 (0xFF)
    uint32_t current_version;  // 当前版本号 (防回滚)
} OTA_Param_t;  // 16 字节，对齐到 Flash 写入单位
```

#### 关键函数

- `bootOTA_ReadParamOTA()`: 从 `configPARAM_ADDRESS` 读取参数，含**向后兼容**逻辑：`active_partition > 1` → 强制改为 0 (旧版本为 `0xFF`)；`current_version == 0xFFFFFFFF` → 重置为 0 (首次启动)。
- `bootOTA_SaveParamOTA()`: 擦除参数扇区后写入新参数，**magic 由本函数统一设置**，调用方不需要关心，保证原子性语义。
- `bootOTA_GetActivePartitionAddr()` / `bootOTA_GetInactivePartitionAddr()`: 根据 `active_partition` 返回对应地址。

### 4.7 签名验证 (sign_verify)

[Bootloader/service/sign_verify/sign_verify.c](Bootloader/service/sign_verify/sign_verify.c) 在固件写入 Flash 后执行 Ed25519 数字签名验证。

#### 验证流程

```
bootSIG_ParseAndVerify(fw_addr, fw_total_size)
  │
  ├─ 1. 检查 fw_total_size ≥ 76 (Footer 最小大小)
  │
  ├─ 2. 从镜像尾部读取 FirmwareFooter_t
  │
  ├─ 3. 校验 footer.footer_size == 76 (自描述大小)
  │
  ├─ 4. 校验 footer.footer_magic == 0xAA55F00D
  │
  ├─ 5. 计算固件本体大小 = fw_total_size - 76 (排除 Footer)
  │
  └─ 6. hydro_sign_verify(signature, fw_body, body_len, "114_514", pubkey)
       │
       └─ 返回 0: 验签通过; ≠0: 验签失败
```

#### 安全设计

- **上下文字符串 "114_514"**: 8 字符的 Ed25519 上下文，Host 端 `binpkg` 工具使用完全相同的字符串签名。任何一方不匹配都导致验证失败。
- **公钥编译进固件**: 硬编码在 `configED25519_PUBKEY` 宏中，不可运行时修改。
- **调试旁路**: `configSIG_VERIFY_ENABLE = 0` 时 `ed25519_verify()` 直接返回 0 (通过)，方便开发调试。
- **随机数生成器**: 固件端使用 `dummy` RNG (`hydro_random_init()` 是空操作)。这**不影响安全性**，因为固件只执行验签（确定性操作，无需随机数），签名操作在上位机完成（使用操作系统真随机数源）。

### 4.8 差量更新 (diff_update + janpatch)

差量更新由 [diff_update](Bootloader/service/diff_update/diff_update.c) (服务层封装) + [janpatch](Bootloader/lib/janpatch/janpatch.h) (算法库) + [janpatch_port](Bootloader/port/janpatch/janpatch_port.c) (Flash 流适配器) 三部分协作完成。

#### 数据流

```
Patch 文件 (OTA 传输到 Sector 4)
       │
       ▼
┌─────────────────────────────────────────────┐
│ janpatch_port                                │
│                                              │
│ FlashStream_t source  (活跃分区, 只读)       │
│ FlashStream_t patch   (Sector 4, 只读)       │
│ FlashStream_t target  (非活跃分区, 写入)      │
│                                              │
│ 页缓冲区 (每个 4096 字节):                    │
│   s_source_page_buf[4096]                    │
│   s_patch_page_buf[4096]                     │
│   s_target_page_buf[4096]                    │
│                                              │
│ 回调: FlashStream_Read_()  → memcpy          │
│       FlashStream_Write_() → bootFlasher_Write│
│       FlashStream_Seek_()  → 偏移量计算       │
└─────────────────────────────────────────────┘
       │
       ▼
  janpatch() 算法引擎 (只依赖流接口，与硬件无关)
       │
       ▼
  非活跃分区 ← 合成后的完整固件
```

#### JANPatch 操作码

| 操作 | 含义 |
|------|------|
| EQL | 从 source 拷贝 N 字节 → target |
| MOD | 从 patch 读取 N 字节修改数据 → target |
| INS | 从 patch 插入 N 字节新数据 → target |
| DEL | 跳过 source 中的 N 字节（不写入 target）|
| BKT | 回溯到 source 之前的某个偏移继续拷贝 |

### 4.9 用户交互菜单 (bootloader_menu)

[Bootloader/service/menu/bootloader_menu.c](Bootloader/service/menu/bootloader_menu.c) 提供串口控制台交互。

#### 触发方式

| 触发条件 | 行为 |
|----------|------|
| PA0 拉低 (硬件触发) | 立即进入有线 Ymodem OTA |
| 倒计时内按 Enter | 进入交互菜单模式 |
| 倒计时到 0 | 自动跳转 App |

#### 菜单选项

```
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
   BootLoader Menu  (~o~)
  -----------------------------------
   1. Jump to Application
   2. Wired Ymodem OTA
   3. LoRa Wireless OTA
   4. Display OTA Parameters
   5. About / Author Info
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
Please select [1-5]:
```

### 4.10 LoRa 无线模块驱动 (lora)

[Bootloader/hardware/lora/lora.c](Bootloader/hardware/lora/lora.c) 驱动 **ATK-MWCC68D** LoRa 无线串口模块。

#### 硬件接口

| 引脚 | 功能 |
|------|------|
| PA9 / PA10 | UART1 TX/RX (连接到 LoRa 模块的 RXD/TXD) |
| PB3 | MD0 (模式控制: HIGH=AT 指令模式 / LOW=数据透传模式) |
| PB4 | AUX (忙指示: HIGH=忙 / LOW=空闲) |

#### 初始化序列

```
bootLoRa_Init()
  │
  ├─ 校验回调函数指针非空
  ├─ MD0 = 0, 等待 AUX = 0 (模块上电就绪)
  ├─ MD0 = 1, 等待 AUX = 0, 等待 800ms (AT 解析器预热)
  ├─ AT 连通性测试
  ├─ AT+UART=115200,0,0      // 串口配置
  ├─ AT+ADDR=0               // 设备地址
  ├─ AT+NETID=0              // 网络 ID
  ├─ AT+WLRATE=5,7           // 空中速率
  ├─ AT+TPOWER=4             // 发射功率
  ├─ AT+CWMODE=0             // 工作模式 (一般/透明)
  ├─ AT+PACKSIZE=3           // 数据包大小 (240 字节)
  ├─ AT+TMODE=0              // 透明传输模式
  ├─ AT+DATAKEY=F1F2F3F4     // 数据加密密钥
  ├─ AT+LBT=0                // 信道检测关闭
  └─ MD0 = 0 (退出 AT 模式 → 透传模式)
```

#### 设计要点

- **完全回调驱动**: LoRa 驱动不包含任何寄存器访问，所有硬件操作通过 `LoRa_Callback_t` 结构体注入。这使得驱动可在不同 UART/GPIO 引脚配置的硬件上复用。
- **AT 响应解析**: 基于行读取 + 超时机制，检查每行是否包含 "OK" 或 "ERROR"。
- **初始化完成后即透明**: 初始化后 LoRa 模块进入透明传输模式，所有 UART 数据直接通过无线发送/接收，Ymodem 协议栈无需感知 LoRa 的存在。

### 4.11 UART DMA 环形缓冲 (uart_dma_ring)

[Bootloader/port/driver/uart/uart_dma_ring.c](Bootloader/port/driver/uart/uart_dma_ring.c) 实现非阻塞的 DMA 环形接收。

#### 工作原理

```
USART1 RX → DMA2 Stream2 Channel 4 (循环模式)
                  │
                  ▼
         rx_buffer[2048]
         ┌──────────────────────┐
         │  ... data stream ... │
         └──────────────────────┘
         ▲                    ▲
    rx_read_ptr         rx_write_ptr
    (消费端)              (生产端/DMA)
```

- **DMA 循环模式**: DMA 自动将 USART1->DR 的数据写入 `rx_buffer`，到达缓冲区末尾后自动回到起始地址。
- **写指针追踪**: `rx_write_ptr = configRX_BUF_SIZE - DMA_GetDataLength()`，利用 DMA 剩余传输计数反算当前位置。
- **回绕处理**: 当 DMA 计数器重置（完成一个整圈），`rx_write_ptr` 从缓冲区末尾跳到 0，处理逻辑通过比较读写指针大小判断如何读取。
- **IDLE 中断**: 当 USART1 检测到线路空闲（一个字节时间内无数据），触发 IDLE 中断作为帧结束信号。Ymodem 协议栈据此判断一包数据是否传输完毕。

### 4.12 Flash 驱动 (flasher)

[Bootloader/port/driver/flasher/flasher.c](Bootloader/port/driver/flasher/flasher.c) 封装 STM32F4xx 内部 Flash 的读写擦操作。

#### Flash 控制器操作

| 操作 | 寄存器流程 |
|------|-----------|
| 解锁 | `FLASH->KEYR = FLASH_KEY1` (0x45670123) → `FLASH->KEYR = FLASH_KEY2` (0xCDEF89AB) |
| 擦除 | 设置 SER → SNB (扇区号) → PSIZE=x32 → STRT → 等待 BSY=0 |
| 编程 | 设置 PG → PSIZE=x8 → 写入 `*addr = data` → 等待 BSY=0 |
| 上锁 | `FLASH->CR \|= FLASH_CR_LOCK` |

#### API 设计

- `bootFlasher_Unlock()` / `bootFlasher_Lock()`: 独立的解锁/上锁接口，供 **janpatch_port** 在批量写入前解锁、写入后上锁。
- `bootFlasher_Write()`: 不含解锁/上锁，由调用方管理锁状态（配合 JANPatch 的批量写入场景）。
- `bootFlasher_WriteByte()`: 便捷封装，自动解锁→写入→上锁，适合 OTA 下载时逐包写入的场景。
- `bootFlasher_ReadData()`: 直接从内存映射地址 `memcpy()`，Flash 对 Cortex-M4 是统一编址的。

### 4.13 密码学库 (libhydrogen)

[Bootloader/lib/hydrogen/](Bootloader/lib/hydrogen/) 是 **libhydrogen** 的嵌入式移植版本，提供轻量级 Ed25519 签名验证。

#### 编译方式

- `hydrogen.c` 聚合包含所有实现文件 (`impl/common.h`, `impl/core.h`, `impl/gimli-core.h`, `impl/hash.h`, `impl/sign.h` 等)，采用 header-only 聚合单文件编译模式。
- 随机数后端: `impl/random/dummy.h`（空实现），原因见 [4.7 节](#47-签名验证-sign_verify)。

#### 使用的 API

```c
// 仅使用验签函数
int hydro_sign_verify(
    const uint8_t csig[64],   // 签名
    const void *msg,           // 原始消息
    size_t msg_len,            // 消息长度
    const char ctx[8],         // 上下文字符串 (8 字节)
    const uint8_t pk[32]       // 公钥 (32 字节)
);
// 返回 0: 验签通过; -1: 验签失败
```

> **注意**: 上位机工具 (`tools/`) 使用独立的、链接了操作系统真随机数源的 libhydrogen 进行签名生成。固件端和上位机端的 libhydrogen 源代码相同，但随机数后端不同。

---

## 5. 上位机工具

项目配套两个 PC 端命令行工具（源码位于 `tools/` 目录，独立构建系统）。

### 5.1 keygen — 密钥生成工具

```
用法: keygen [-o <prefix>]

功能:
  1. 生成 Ed25519 密钥对 (公钥 32B + 私钥 64B)
  2. 以十六进制打印到 stdout
  3. -o 选项输出 .pub / .key 文件
  4. 打印可直接粘贴到 configBootloader.h 的 C 数组格式

示例:
  $ keygen -o my_key
  Public Key (for configBootloader.h):
  #define configED25519_PUBKEY { \
      0x92, 0x57, 0xE1, ...      \
  }
  (同时生成 my_key.pub 和 my_key.key)
```

### 5.2 binpkg — 固件签名打包工具

```
用法: binpkg -i <firmware.bin> -k <secret.hex> -v <version> [-o <output.bin>] [--in-place]

功能:
  1. 读取原始固件二进制文件
  2. 读取十六进制私钥文件
  3. 使用 hydro_sign_create() 对固件签名 (context="114_514")
  4. 构造 FirmwareFooter_t (版本 + 签名 + magic + size)
  5. 将 Footer 追加到固件末尾
  6. 输出签名后的完整固件

示例:
  $ binpkg -i app_v2.bin -k my_key.key -v 2 -o app_v2_signed.bin
  $ binpkg -i app_v2.bin -k my_key.key -v 2 --in-place
```

#### 输出固件格式

```
[raw firmware binary (N bytes)] [76-byte FirmwareFooter_t]
```

### 5.3 共享头文件

`tools/core/firmware_footer.h` 定义的 `FirmwareFooter_t` 必须与固件端 `sign_verify.h` 中的定义**逐字节一致**：

- `FW_SIGNATURE_SIZE` = 64
- `FW_FOOTER_MAGIC` = `0xAA55F00D`
- `FW_SIGN_CONTEXT` = `"114_514"`
- `FW_FOOTER_SIZE` = 76
- 包含编译期静态断言: `_Static_assert(sizeof(FirmwareFooter_t) == 76)`

---

## 6. 关键设计决策

### 6.1 为什么用 LL 库而非 HAL 库？

- **体积**: HAL 库代码量大，BootLoader 仅 32KB Flash，体积敏感。
- **控制**: 直接操作寄存器，减少抽象层开销，启动更快。
- **适合简单外设**: BootLoader 仅使用 GPIO、USART、DMA，LL 库完全满足需求。

### 6.2 为什么用 goto 状态机？

- **栈深度可预测**: 避免在深层嵌套函数中跳转的栈膨胀问题。
- **代码集中**: 整个主流程可视在 `main()` 中，调试和审查方便。
- **历史决策**: 项目早期面临栈溢出问题，goto 方案解决了该问题。

### 6.3 为什么固件端用 dummy RNG？

- **不需要**: 固件只验签 (Ed25519 verify)，不需要随机数——验签是纯确定性计算。
- **省体积**: 省略随机数生成器实现可减少代码体积。
- **签名在 PC 端**: 签名生成 (需要真随机数) 在上位机 `binpkg` 工具完成，使用 OS 的 `/dev/urandom` 等。

### 6.4 为什么链接时丢弃标准库？

- libc/libm/libgcc 的代码即使不使用也会引入几百字节的启动/清理代码。
- BootLoader 中 printf 通过重写 `_write()` 直接操作 UART，不依赖 libc 的缓冲 I/O。
- 手动实现 `atoi()` 等效逻辑、`strcmp()` 等效逻辑，避免链接重型库函数。

### 6.5 为什么 Footer 放在固件末尾？

- **流式处理**: 固件通过 Ymodem 流式接收，Footer 在最后到达。先写完固件本体，再解析末尾的 Footer，符合数据到达的自然顺序。
- **标准兼容**: 去掉 76 字节 Footer 后，固件本体是标准 ARM Cortex-M 二进制 (起始于 SP+PC 向量)，可直接烧录调试。
- **差量友好**: 差量补丁可以只 Patch 固件本体，Footer 由新版本 binpkg 重新生成。

---

## 7. OTA 升级完整流程

### 全量升级 (Wired UART)

```
┌──────────┐                          ┌─────────────┐
│  PC 端    │                          │ STM32F411   │
│ (发送端)  │                          │ (BootLoader) │
└────┬─────┘                          └──────┬──────┘
     │                                       │
     │  1. 上电/复位                          │
     │     PA0 拉低 / 倒计时内按 Enter        │
     │                                       │
     │  2. 选择菜单项 "2. Wired Ymodem OTA"   │
     │                                       │
     │  3. Ymodem 连接建立 ('C' → Packet 0)  │
     │◄──────────────────────────────────────│
     │                                       │
     │  4. 发送 firmware_signed.bin          │
     │     (固件本体 + 76B Footer)            │
     │──────────────────────────────────────►│
     │                                       │  写入非活跃分区
     │                                       │
     │  5. 传输结束 (EOT × 2)                 │
     │◄─────────────────────────────────────►│
     │                                       │
     │                                       │  6. 解析 Footer
     │                                       │     - 验证 version
     │                                       │     - 验证 signature
     │                                       │     - 防回滚检查
     │                                       │  7. 计算 CRC16
     │                                       │  8. 保存 OTA 参数
     │                                       │     (切换活跃分区)
     │                                       │
     │                                       │  9. 跳转到新固件
     │                                       │     Bootloader_JumpToApp()
```

### 差量升级

与全量升级的区别仅在于第 4-6 步：

```
4. 发送 patch_signed.bin → 写入 Patch Storage (Sector 4)
5. 传输结束
6. 差量合成:
   ┌──────────────┐
   │ 活跃分区固件  │ ──┐
   └──────────────┘   │
                      ├── janpatch() → 非活跃分区 (新固件)
   ┌──────────────┐   │
   │ Sector 4 补丁 │ ──┘
   └──────────────┘
7. 对新固件执行签名验证 + CRC + 保存参数 + 跳转
```

### LoRa 无线 OTA

流程与有线完全一致，仅在物理层不同：

- 初始化时 `bootLoRa_Init()` 完成 AT 配置，模块进入透传模式。
- Ymodem 的每个字节通过 LoRa 模块在空中透明转发。
- Ymodem 超时参数更长（字节超时 500ms，包超时 5000ms）以适应无线延迟。
- 菜单选择 "3. LoRa Wireless OTA" 进入。

---

## 附录: 模块依赖关系图

```
main.c
 ├── configBootloader.h      (编译时配置)
 ├── bootloader.h             (跳转 App)
 ├── uart_dma_ring.h          (UART DMA 环形缓冲)
 ├── ymodem.h                 (Ymodem 协议栈)
 ├── flasher.h                (Flash 驱动)
 ├── ota.h                    (A/B 分区管理)
 ├── ota_download.h           (OTA 下载编排器)
 │    ├── ymodem.h
 │    ├── ota.h
 │    ├── sign_verify.h       (Ed25519 签名验证)
 │    │    └── hydrogen.h      (libhydrogen 密码库)
 │    └── diff_update.h       (差量更新入口)
 │         └── janpatch_port.h (Flash 流适配器)
 │              ├── janpatch.h (JANPatch 算法库)
 │              └── flasher.h
 ├── bootloader_menu.h        (交互菜单)
 └── lora.h                   (LoRa 模块 AT 驱动)

上位机工具 (独立构建):
 binpkg + keygen
  ├── firmware_footer.h       (共享 Footer 定义)
  └── hydrogen.h              (主机版 libhydrogen, 含真随机数)
```

---

> **文档版本**: 1.0

> **最后更新**: 2026-06-10
