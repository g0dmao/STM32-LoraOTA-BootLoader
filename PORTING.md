# STM32-LoraOTA-BootLoader 移植指南 (基于 STM32CubeMX)

> **适用场景**: 将 STM32-LoraOTA-BootLoader 从 STM32F411CEU6 移植到其他 STM32 系列 MCU

> **先决条件**: 已阅读 [技术实现文档 (README.md)](README.md) 理解项目架构

****该文档由 DeepSeek-v4-pro 生成，部分内容未经验证***

---

## 目录

1. [移植概览](#1-移植概览)
2. [第一步: CubeMX 新建裸机工程](#第一步-cubemx-新建裸机工程)
3. [第二步: 修改链接脚本](#第二步-修改链接脚本)
4. [第三步: 复制 BootLoader 源码](#第三步-复制-bootloader-源码)
5. [第四步: 适配 Flash 分区布局](#第四步-适配-flash-分区布局)
6. [第五步: 适配 Flash 驱动](#第五步-适配-flash-驱动)
7. [第六步: 适配 UART DMA 驱动](#第六步-适配-uart-dma-驱动)
8. [第七步: 适配 LoRa 模块](#第七步-适配-lora-模块)
9. [第八步: 在 main.c 中装配启动逻辑](#第八步-在-mainc-中装配启动逻辑)
10. [第九步: 修改 CMakeLists.txt](#第九步-修改-cmakeliststxt)
11. [第十步: 各系列差异速查](#第十步-各系列差异速查)
12. [第十一步: 验证测试](#第十一步-验证测试)
13. [常见问题排查](#常见问题排查)

---

## 1. 移植概览

### 你面对的是什么

本项目 BootLoader 已经做好了硬件解耦——**Component 层** 和 **Service 层**（约 80% 的代码）完全不碰寄存器，只通过回调函数指针与硬件交互。移植时你只需做三件事：

1. **CubeMX 帮你搞定**: 时钟树、GPIO、USART、DMA、启动文件——全自动生成。
2. **你改 3 个文件**: `configBootloader.h`（分区地址）、`flasher.c`（Flash 擦写）、`uart_dma_ring.c`（DMA 实例名）。
3. **你装配 main.c**: 把回调绑定 + 状态机代码写入 CubeMX 生成的 main.c。

### 文件命运速查

| 操作 | 文件 |
|------|------|
| **CubeMX 自动生成，直接使用** | `startup_*.s`, `*.ld`, `system_*.c`, `*_it.c`, `gpio.c`, `dma.c`, `usart.c`, `main.h`, `cmake/stm32cubemx/` |
| **从原项目完整复制** | `Bootloader/` 全部子目录（component, service, lib, port, hardware）|
| **复制后需修改** | `configBootloader.h`, `flasher.c`, `uart_dma_ring.c`, `bootloader.c` |
| **CubeMX 生成后需填入** | `main.c` — 加入回调函数 + 启动状态机 |
| **需手动编辑** | `CMakeLists.txt` — 加入 BootLoader 源文件和编译选项 |

---

## 2. 第一步: CubeMX 新建裸机工程

为目标 MCU 新建工程，配置必要外设。

### 2.1 外设配置

| 外设 | 配置 | 用途 |
|------|------|------|
| **RCC** | HSE Crystal + PLL | 系统时钟 |
| **SYS** | SysTick | 1ms 时基 |
| **USART1** | 115200 8N1, **DMA RX 使能 (Circular)** | 通信/日志 |
| **GPIO Input ×1** | 上拉输入 (如 PA0) | OTA 触发引脚 |
| **GPIO Output ×1** | 推挽输出 (如 PB3) | LoRa MD0 (如需要 LoRa) |
| **GPIO Input ×1** | (如 PB4) | LoRa AUX (如需要 LoRa) |

### 2.2 生成选项

- **Toolchain**: CMake
- **固件库**: 勾选 LL (Low Layer)，不使用 HAL（控制体积）

### 2.3 编译验证

```bash
cd <new_project>
cmake --preset <your_preset>
cmake --build build
# 烧录验证
```

---

## 3. 第二步: 修改链接脚本

CubeMX 生成的 `*.ld` 文件中，**Flash 的 LENGTH 是芯片总大小**。BootLoader 只占前面一小段，需要改为 BootLoader 实际占用的空间。

### 3.1 修改 MEMORY 段

```ld
/* CubeMX 生成的是芯片总 Flash 大小 (如 512K、256K...) */
/* 改为 BootLoader 占用的大小: */

MEMORY
{
RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = <芯片 RAM 大小，不用改>
FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = <BootLoader 大小，如 32K、16K>
}
```

BootLoader 大小的确定：看分区规划中 BootLoader 占了多少扇区/页，换算成 KB。例如占 Sector 0 (16KB)，则 `LENGTH = 16K`。

### 3.2 添加 DISCARD 段

在 SECTIONS 末尾添加（丢弃标准库以减小体积）：

```ld
  /DISCARD/ :
  {
    libc.a:* ( * )
    libm.a:* ( * )
    libgcc.a:* ( * )
  }
```

---

## 4. 第三步: 复制 BootLoader 源码

将原项目整个 `Bootloader/` 目录复制到新项目的根目录下：

```
原项目/Bootloader/  →  新项目/Bootloader/
```

包含以下完整目录树：

```
Bootloader/
├── configBootloader.h              ← (后续需适配)
├── component/
│   ├── bootloader/
│   └── ota_download/
├── service/
│   ├── ymodem/
│   ├── ota/
│   ├── sign_verify/
│   ├── diff_update/
│   └── menu/
├── port/
│   ├── driver/
│   │   ├── flasher/                ← (后续需适配)
│   │   └── uart/                   ← (后续需适配)
│   └── janpatch/
├── hardware/
│   └── lora/
└── lib/
    ├── hydrogen/
    └── janpatch/
```

---

## 5. 第四步: 适配 Flash 分区布局

**文件**: `Bootloader/configBootloader.h`

### 5.1 确认目标 MCU 的 Flash 规格

查阅数据手册，弄清：
- Flash 总大小
- 擦除单元类型和大小：F4 用**扇区** (16KB/64KB/128KB)，F1/G0/G4/L4 用**页** (1KB/2KB)
- 编程宽度：F4 可选、F1 固定 16-bit、G0/G4/L4 固定 64-bit

### 5.2 规划分区

| Flash 总量 | BootLoader | OTA Param | Patch | Part A/B |
|-----------|-----------|----------|-------|----------|
| 512KB (原方案) | 32KB | 16KB | 64KB | 128KB×2 |
| 256KB | 32KB | 8KB | 32KB | 64KB×2 |
| 128KB | 16KB | 4KB | 16KB | 32KB×2 |
| 64KB | 8KB | 2KB | — | 20KB×2 |

**约束**:
- 每个分区独占完整的擦除单元
- 各分区地址不能重叠
- F1/G0/G4/L4 以页为单位，`*_SECTOR` 实际填**页号**

### 5.3 修改以下宏

```c
#define configPART_A_ADDRESS       0x08040000  // →
#define configPART_A_SECTOR        6           // →
#define configPART_A_SECTOR_NUM    1           // → 如需多扇区

#define configPART_B_ADDRESS       0x08020000  // →
#define configPART_B_SECTOR        5           // →
#define configPART_B_SECTOR_NUM    1

#define configAPP_MAX_SIZE         (128*1024)  // →

#define configPARAM_ADDRESS        0x08008000  // →
#define configPARAM_SECTOR         2           // →
#define configPARAM_SECTOR_NUM     1

#define configPATCH_STORAGE_ADDRESS    0x08010000  // →
#define configPATCH_STORAGE_SECTOR     4           // →
#define configPATCH_STORAGE_SECTOR_NUM 1
#define configPATCH_MAX_SIZE           (64*1024)   // →

// 页缓冲区 (与 Flash 页大小对齐或减半)
#define configJP_SOURCE_PAGE_SIZE      4096  // →
#define configJP_PATCH_PAGE_SIZE       4096  // →
#define configJP_TARGET_PAGE_SIZE      4096  // →
```

---

## 6. 第五步: 适配 Flash 驱动

**文件**: `Bootloader/port/driver/flasher/flasher.c`

### 6.1 F4 → F4 同系列

Flash 控制器完全相同，**此文件直接使用，跳过此步**。

### 6.2 不同系列的适配

不同 STM32 系列的核心差异是**编程宽度**和**擦除方式**。其余（解锁/上锁/等待/读/写标志）结构都一样。

**STM32F1 系列** — 固定 16-bit 半字编程 + 页擦除，用 AR 寄存器指定地址：

```c
// F1 写入:
int8_t bootFlasher_Write(uint32_t address, uint8_t *data, uint16_t length)
{
    flash_clear_flags();
    FLASH->CR |= FLASH_CR_PG;
    for (uint16_t i = 0; i < length; i += 2) {
        uint16_t half = data[i] | ((i+1 < length) ? ((uint16_t)data[i+1] << 8) : 0xFF00);
        *(__IO uint16_t *)(address + i) = half;
        if (flash_wait_done() != 0) { FLASH->CR &= ~FLASH_CR_PG; return -1; }
    }
    FLASH->CR &= ~FLASH_CR_PG;
    return 0;
}

// F1 页擦除:
int8_t bootFlasher_EraseSectors(int page, int page_count)
{
    bootFlasher_Unlock();
    flash_clear_flags();
    for (int i = 0; i < page_count; i++) {
        FLASH->CR |= FLASH_CR_PER;
        FLASH->AR = page_addr(page + i);     // ← F1 特有
        FLASH->CR |= FLASH_CR_STRT;
        if (flash_wait_done() != 0) { FLASH->CR &= ~FLASH_CR_PER; bootFlasher_Lock(); return -1; }
    }
    FLASH->CR &= ~FLASH_CR_PER;
    bootFlasher_Lock();
    return 0;
}
```

**STM32G0/G4/L4 系列** — 固定 64-bit 双字编程 + 页擦除：

```c
// G0/G4/L4 写入:
int8_t bootFlasher_Write(uint32_t address, uint8_t *data, uint16_t length)
{
    flash_clear_flags();
    FLASH->CR |= FLASH_CR_PG;
    for (uint16_t i = 0; i < length; i += 8) {
        uint64_t dword = 0;
        for (int j = 0; j < 8 && (i+j) < length; j++)
            ((uint8_t *)&dword)[j] = data[i+j];
        *(__IO uint64_t *)(address + i) = dword;
        if (flash_wait_done() != 0) { FLASH->CR &= ~FLASH_CR_PG; return -1; }
    }
    FLASH->CR &= ~FLASH_CR_PG;
    return 0;
}

// G0/G4 页擦除:
int8_t bootFlasher_EraseSectors(int page, int page_count)
{
    bootFlasher_Unlock();
    flash_clear_flags();
    for (int i = 0; i < page_count; i++) {
        FLASH->CR |= FLASH_CR_PER;
        FLASH->CR |= ((page + i) << FLASH_CR_PNB_Pos);  // ← G0/G4 特有
        FLASH->CR |= FLASH_CR_STRT;
        if (flash_wait_done() != 0) { FLASH->CR &= ~FLASH_CR_PER; bootFlasher_Lock(); return -1; }
    }
    FLASH->CR &= ~FLASH_CR_PER;
    bootFlasher_Lock();
    return 0;
}
```

> 通用部分（`flash_clear_flags`、`flash_wait_done`、`bootFlasher_Unlock`、`bootFlasher_Lock`、`bootFlasher_ReadData`）在 STM32 全系列结构相同，直接从原项目复制后，对照目标 MCU 的头文件确认寄存器名和标志位即可。

---

## 7. 第六步: 适配 UART DMA 驱动

**文件**: `Bootloader/port/driver/uart/uart_dma_ring.c`

### 7.1 F4 → F4 同系列

DMA 寄存器映射相同，直接使用，跳过此步。

### 7.2 不同系列

只需替换 DMA 实例名。在 CubeMX 生成的 `dma.c` / `usart.c` 中找到 USART1_RX 绑定的 DMA，把文件中的 `DMA2`、`LL_DMA_STREAM_2` 替换为对应实例：

```c
// 原 (F411, USART1_RX = DMA2 Stream2 Channel4):
LL_DMA_GetDataLength(DMA2, LL_DMA_STREAM_2)

// 替换示例 (F103, USART1_RX = DMA1 Channel5):
LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_5)
```

USART 的 LL API (`LL_USART_IsActiveFlag_TXE` 等) 在 STM32 全系列通用，无需修改。

### 7.3 无 DMA 替代方案

如果 MCU 没有 DMA，用中断环形缓冲替代（`bootUART_ReadByte` 和 `bootUART_SendByte` 的接口保持不变）：

```c
#define RX_BUF_SIZE 2048
static volatile uint8_t  rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head, rx_tail;

void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t data = USART1->DR;
        uint16_t next = (rx_head + 1) % RX_BUF_SIZE;
        if (next != rx_tail) { rx_buf[rx_head] = data; rx_head = next; }
    }
}

uint8_t bootUART_ReadByte(uint8_t *pData)
{
    if (rx_head == rx_tail) return 0;
    *pData = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return 1;
}
```

---

## 8. 第七步: 适配 LoRa 模块

本项目原配 **ATK-MWCC68D** LoRa 无线串口模块。LoRa 驱动代码通过回调函数与硬件交互，具有较好的可移植性。以下分别说明与原模块相同、使用其他 AT 指令模块、使用 SPI 接口模块三种场景的移植方法。

### 8.1 硬件连接模型

```
  MCU                           LoRa 模块
┌──────────┐          ┌──────────────────────┐
│  UART TX │─────────►│ RXD                   │
│  UART RX │◄─────────│ TXD                   │
│  GPIO O  │─────────►│ MD0/Config (模式控制)  │
│  GPIO I  │◄─────────│ AUX/Busy  (忙指示)     │
│  GND     │─────────│ GND                    │
│  3.3V    │─────────│ VCC                    │
└──────────┘          └──────────────────────┘
```

LoRa 模块与 BootLoader 共用一个 UART。模块进入透传模式后，PC 通过无线发送的 Ymodem 数据与有线直连走的是**同一路 UART 数据流**，对上层协议完全透明。

### 8.2 场景一: 使用与 ATK-MWCC68D 相同的模块

原项目 `lora.c` 直接可用，**驱动代码一行不改**。只需：

#### CubeMX GPIO 配置

| GPIO | 模式 | User Label | 说明 |
|------|------|-----------|------|
| MD0 (如 PB3) | Output Push Pull | `MD0` | 初始低电平 |
| AUX (如 PB4) | Input | `AUX` | 读忙状态 |

#### main.c 回调绑定

```c
#if(configLORA)
/* User Label = "MD0" */
static void LoRa_SetMD0(uint8_t level)
{
    if (level) LL_GPIO_SetOutputPin(MD0_GPIO_Port, MD0_Pin);
    else       LL_GPIO_ResetOutputPin(MD0_GPIO_Port, MD0_Pin);
}

/* User Label = "AUX" */
static uint8_t LoRa_ReadAUX(void)
{
    return (LL_GPIO_ReadInputPort(AUX_GPIO_Port) & AUX_Pin) ? 1 : 0;
}

/* 在 BootloaderInit() 中绑定 */
g_lora_cb.set_md0_cb   = LoRa_SetMD0;
g_lora_cb.read_aux_cb  = LoRa_ReadAUX;
#endif
```

不需要 LoRa 时，在 `configBootloader.h` 设 `#define configLORA No` 即可完全禁用。

### 8.3 场景二: 使用其他 AT 指令串口模块

许多 LoRa 模块（如 E32、E220、E22 等）和 ATK-MWCC68D 一样，也是 **UART 接口 + AT 指令配置 + 透传模式**。这类模块移植时**不需要修改上层代码**，只需修改 `lora.c` 中的 AT 指令集和配置逻辑。

#### 需要修改的文件

仅 `Bootloader/hardware/lora/lora.c` 中的 `bootLoRa_SetConfig_()` 函数。

#### 原 ATK-MWCC68D 的配置序列

位于 `lora.c` 的 `bootLoRa_SetConfig_()` 函数中，核心是一组 AT 指令发送：

```c
// 原代码结构 (简化):
bootLoRa_SendCmd("AT");              // 连通性测试
bootLoRa_SendCmd("AT+UART=...");     // 串口参数
bootLoRa_SendCmd("AT+ADDR=...");     // 地址
bootLoRa_SendCmd("AT+NETID=...");    // 网络 ID
bootLoRa_SendCmd("AT+WLRATE=...");   // 空中速率
bootLoRa_SendCmd("AT+TPOWER=...");   // 发射功率
bootLoRa_SendCmd("AT+CWMODE=...");   // 工作模式
bootLoRa_SendCmd("AT+PACKSIZE=..."); // 包大小
bootLoRa_SendCmd("AT+TMODE=...");    // 透传模式
bootLoRa_SendCmd("AT+DATAKEY=...");  // 加密密钥
bootLoRa_SendCmd("AT+LBT=...");      // 信道检测
```

#### 适配方法

根据新模块的 AT 指令手册，替换指令字符串、参数枚举值、以及可能需要增删的配置项：

```c
// 示例: 适配到某 E32 系列模块
static int8_t bootLoRa_SetConfig_(LoRa_Config_t *cfg)
{
    char buf[64];

    // 1. 连通性测试 (通常所有 AT 模块都支持)
    if (bootLoRa_SendCmd("AT") != 0) return -1;

    // 2. 波特率和校验 (指令格式可能不同)
    //    原: AT+UART=<baud>,<parity>
    //    E32: AT+UART=<baud>,<parity>  (相同则不改)
    sprintf(buf, "AT+UART=%d,%d", cfg->baud_rate, cfg->parity);
    if (bootLoRa_SendCmd(buf) != 0) return -1;

    // 3. 地址 (指令名可能不同)
    //    原: AT+ADDR=<addr>
    //    E32: AT+ADDRESS=<addr>
    sprintf(buf, "AT+ADDRESS=%d", cfg->addr);
    if (bootLoRa_SendCmd(buf) != 0) return -1;

    // 4. 空中速率 (参数编码可能不同)
    //    根据新模块手册重新映射 LORA_AIR_RATE_* 的值
    sprintf(buf, "AT+WLRATE=%d,%d", cfg->air_rate, /* ... */);
    if (bootLoRa_SendCmd(buf) != 0) return -1;

    // 5. 信道 (频率映射可能不同)
    //    原: 410MHz + channel
    //    新模块: 查看手册的频道-频率对照表
    sprintf(buf, "AT+CHANNEL=%d", cfg->channel);
    if (bootLoRa_SendCmd(buf) != 0) return -1;

    // ... 继续适配其余指令 ...

    // 最后: 进入透传模式 (指令名可能不同)
    //    原: AT+TMODE=0
    //    某些模块不需要此步骤, 退出 AT 模式即进入透传
    return 0;
}
```

#### 需要同步更新的定义

`lora.h` 中的配置枚举可能需要重新定义，以匹配新模块的参数编码：

```c
// 如果新模块的空中速率编码与原模块不同, 修改这些:
#define LORA_AIR_RATE_1_2Kbps   0   // ← 按新模块手册
#define LORA_AIR_RATE_2_4Kbps   2
// ...

// 如果新模块没有某些功能 (如 DATAKEY), 可以:
//   1. 在 lora.h 中删除对应配置项
//   2. 在 LoRa_Config_t 中删除对应字段
//   3. 在 bootLoRa_SetConfig_() 中删除对应指令发送
```

#### 模式控制 (MD0/AUX) 的适配

不同模块的模式切换机制可能不同：

| 模块 | 进入 AT 模式 | 退出 (透传) | 忙指示 |
|------|------------|------------|-------|
| ATK-MWCC68D | MD0=HIGH | MD0=LOW | AUX=HIGH 为忙 |
| E32 系列 | MD0=HIGH | MD0=LOW | AUX=LOW 为忙(*) |
| E220 系列 | MD0=HIGH | MD0=LOW | AUX=LOW 为空闲 |

> (*) 部分模块 AUX 逻辑反相，需修改 `bootLoRa_IsBusy()` 和 `WaitForAuxLow_()` 中的判断逻辑。

如果新模块**完全不需要** MD0/AUX 引脚（如通过指令进入透传），则无需配置这两个 GPIO，`set_md0_cb` 和 `read_aux_cb` 可以为空实现：

```c
// 无需 MD0/AUX 的模块
static void LoRa_SetMD0(uint8_t level) { (void)level; }
static uint8_t LoRa_ReadAUX(void) { return 0; }
```

但 `WaitForAuxLow_()` 需要用延时替代（根据模块手册给出的上电/模式切换等待时间）。

### 8.4 场景三: 使用 SPI 接口的 LoRa 模块

部分 LoRa 模块（如 SX1278、SX1262 等）使用 **SPI 接口**而非 UART，这类模块的驱动需要**完全重写**。

#### 工作量估算

| 需要修改 | 说明 |
|---------|------|
| `lora.c` / `lora.h` | 全部重写 |
| `main.c` 的 `BootloaderInit()` | 调用新驱动的初始化函数 |
| `configBootloader.h` | 可保留 `configLORA` 开关 |
| `LoRa_Callback_t` | 可能需要重新设计（SPI 不是字节流） |

#### 重写思路

SPI 接口的 LoRa 模块驱动程序需要做的事情与原驱动相同：

1. **初始化**: 通过 SPI 配置模块的寄存器（频率、功率、速率、模式等）
2. **收发**: 将上层的数据包通过 SPI 发送/接收
3. **对外接口**: 向上提供 "发一个字节" / "收一个字节" 的接口，使 Ymodem 协议栈无感知

关键：无论底层是 UART 还是 SPI，最终对上层暴露的仍是 `read_byte_cb` / `send_byte_cb` 这样的**字节流接口**，BootLoader 的其余代码（Ymodem、OTA 下载等）完全不需要改动。

#### 通用步骤

```
1. 新建 lora_spi.c / lora_spi.h
   ├── 写 SPI 寄存器读写函数 (读-修改-写模式)
   ├── 实现模块配置函数 (替代原 AT 指令序列)
   │   ├── 设置载波频率 (寄存器 Frf)
   │   ├── 设置发射功率 (寄存器 PaConfig / Ocp)
   │   ├── 设置空中速率 (寄存器 ModemConfig)
   │   ├── 设置数据包格式 (寄存器 PacketConfig)
   │   └── 设置工作模式 (寄存器 OpMode: SLEEP → STANDBY → TX/RX)
   ├── 实现收发函数
   │   ├── 发送: 写 FIFO → 触发 TX → 等待 TX Done
   │   └── 接收: 进入 RX 模式 → 等待 RX Done → 读 FIFO
   └── 对外提供 bootLoRa_Init() / bootLoRa_ExitATMode() 等 API (保持接口不变)

2. 更新 LoRa_Callback_t (如需要)
   ├── SPI 模块通常不需要 MD0/AUX 回调
   └── 可能需要 SPI CS、DIO0/DIO1 中断等新回调

3. 在 main.c 中注入新的 SPI 相关回调
```

#### 接口兼容性建议

保持与原驱动相同的对上层 API，使 `main.c` 的调用代码不变：

```c
// 对外接口保持不变:
int8_t  bootLoRa_Init(LoRa_Callback_t *cb);  // 初始化配置
void    bootLoRa_ExitATMode(void);            // 进入工作模式 (对于 SPI 模块即进入 RX Ready)
uint8_t bootLoRa_IsBusy(void);               // 查询模块状态
```

如果 SPI 模块的 `LoRa_Callback_t` 需要额外回调（如 SPI CS 控制、外部中断引脚等），扩展该结构体即可：

```c
typedef struct LoRa_Callback {
    uint8_t (*read_byte_cb)(uint8_t *pData);   // 不变
    void    (*send_byte_cb)(uint8_t data);      // 不变
    uint32_t (*get_tick_cb)(void);              // 不变
    void    (*set_md0_cb)(uint8_t level);       // 可能不需要了
    uint8_t (*read_aux_cb)(void);              // 可能不需要了
    // SPI 模块新增:
    void    (*spi_cs_cb)(uint8_t level);        // SPI 片选
    uint8_t (*spi_transfer_cb)(uint8_t data);   // SPI 单字节传输
    void    (*delay_us_cb)(uint32_t us);        // 微秒延时
} LoRa_Callback_t;
```

### 8.5 不需要 LoRa 时

在 `configBootloader.h` 中设置：

```c
#define configLORA    No
```

所有 LoRa 相关代码将被条件编译排除，CubeMX 中也不需要配置 MD0 和 AUX 引脚。

---

## 9. 第八步: 在 main.c 中装配启动逻辑

在 CubeMX 生成的 `Core/Src/main.c` 中，加入 BootLoader 的启动代码。需要做四件事：添加头文件、添加全局变量、实现回调函数、在 main() 中插入状态机。

### 9.1 添加头文件

在 `/* USER CODE BEGIN Includes */` 和 `/* USER CODE END Includes */` 之间加入：

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "configBootloader.h"
#include "bootloader.h"
#include "uart_dma_ring.h"
#include "ymodem.h"
#include "flasher.h"
#include "ota.h"
#include "ota_download.h"
#include "bootloader_menu.h"
#if(configLORA)
#include "lora.h"
#endif
/* USER CODE END Includes */
```

### 9.2 添加全局变量和函数声明

```c
/* USER CODE BEGIN PV */
YM_InfoBlock_t     g_ym_ctx;
OTA_Context_t      g_ota_ctx;
BootMenu_Context_t g_menu_ctx;
#if(configLORA)
LoRa_Callback_t    g_lora_cb;
#endif
extern volatile uint32_t g_sys_tick;
/* USER CODE END PV */

/* USER CODE BEGIN PFP */
void CloseAllPeripheral(void);
static void   BootloaderInit(void);
static uint32_t GetTick(void);
static uint8_t  ReadOtaPin(void);
#if(configLORA)
static void   LoRa_SetMD0(uint8_t level);
static uint8_t LoRa_ReadAUX(void);
#endif
/* USER CODE END PFP */
```

### 9.3 实现回调函数

将 BootLoader 的抽象接口与 CubeMX 生成的 LL 外设函数绑定。**GPIO 宏名取决于 CubeMX 中给引脚分配的 User Label**（以下用 `OTA`、`MD0`、`AUX` 作为 User Label 示例，实际替换为你的命名）。

```c
/* USER CODE BEGIN 4 */

/* printf 重定向到 USART1 */
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++) {
        while (!LL_USART_IsActiveFlag_TXE(USART1));
        LL_USART_TransmitData8(USART1, ptr[i]);
    }
    return len;
}

static uint32_t GetTick(void) { return g_sys_tick; }

/* OTA 触发引脚 (User Label = "OTA") */
static uint8_t ReadOtaPin(void)
{
    return (LL_GPIO_ReadInputPort(OTA_GPIO_Port) & OTA_Pin) ? 0 : 1;
}

#if(configLORA)
/* LoRa MD0 引脚 (User Label = "MD0") */
static void LoRa_SetMD0(uint8_t level)
{
    if (level)
        LL_GPIO_SetOutputPin(MD0_GPIO_Port, MD0_Pin);
    else
        LL_GPIO_ResetOutputPin(MD0_GPIO_Port, MD0_Pin);
}

/* LoRa AUX 引脚 (User Label = "AUX") */
static uint8_t LoRa_ReadAUX(void)
{
    return (LL_GPIO_ReadInputPort(AUX_GPIO_Port) & AUX_Pin) ? 1 : 0;
}
#endif

/* 跳转前反初始化外设 (实例名根据 CubeMX 生成的实际外设填写) */
void CloseAllPeripheral(void)
{
    LL_USART_DeInit(USART1);
    LL_DMA_DeInit(DMA2, LL_DMA_STREAM_2);  // ← 按实际 DMA 实例修改
    LL_GPIO_DeInit(GPIOA);                  // ← 按实际使用的 GPIO 端口
}

static void BootloaderInit(void)
{
    /* Ymodem I/O */
    g_ym_ctx.read_byte_cb = bootUART_ReadByte;
    g_ym_ctx.send_byte_cb = bootUART_SendByte;
    g_ym_ctx.get_tick_cb  = GetTick;

    /* Flash 操作 */
    g_ota_ctx.read_cb   = bootFlasher_ReadData;
    g_ota_ctx.write_cb  = bootFlasher_WriteByte;
    g_ota_ctx.erase_cb  = bootFlasher_EraseSectors;
    g_ota_ctx.unlock_cb = bootFlasher_Unlock;
    g_ota_ctx.lock_cb   = bootFlasher_Lock;

    bootUART_RegisterTransmitPort();

#if(configLORA)
    g_lora_cb.read_byte_cb = bootUART_ReadByte;
    g_lora_cb.send_byte_cb = bootUART_SendByte;
    g_lora_cb.get_tick_cb  = GetTick;
    g_lora_cb.set_md0_cb   = LoRa_SetMD0;
    g_lora_cb.read_aux_cb  = LoRa_ReadAUX;

    /* LoRa 模块初始化，最多重试 3 次 */
    int8_t retry = 3 + 1;
    int ret;
    do {
        if (retry <= 0) for(;;);    // 初始化失败，死循环
        ret = bootLoRa_Init(&g_lora_cb);
        retry--;
    } while (ret != 0);
#endif

    g_menu_ctx.read_byte_cb    = bootUART_ReadByte;
    g_menu_ctx.read_ota_pin_cb = ReadOtaPin;
}

/* USER CODE END 4 */
```

### 9.4 在 main() 中插入状态机

在 CubeMX 生成的 `main()` 函数中，外设初始化完成后，用 BootLoader 启动序列替换 `while(1)`：

```c
int main(void)
{
    /* CubeMX 生成的初始化 */
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();

    /* ---- BootLoader 启动序列 ---- */
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;

    BootloaderInit();
    BootMenu_PrintBanner();

    OTA_Param_t param;

    uint32_t start_time = GetTick();
    for (;;)
    {
        BootMenu_Action_t action = BootMenu_Poll(&g_menu_ctx, GetTick() - start_time);

        if (action == BOOTMENU_ACTION_ENTER_OTA)
        {
            if (OtaDownload_Execute(&g_ym_ctx, &g_ota_ctx, &param) == OTADL_STATUS_OK)
                goto Jump;
            else goto err;
        }
        if (action == BOOTMENU_ACTION_ENTER_LORA_OTA)
        {
#if(configLORA)
            bootLoRa_ExitATMode();
#endif
            if (OtaDownload_Execute(&g_ym_ctx, &g_ota_ctx, &param) == OTADL_STATUS_OK)
                goto Jump;
            else goto err;
        }
        if (action == BOOTMENU_ACTION_JUMP_APP)   goto Check;
        if (action == BOOTMENU_ACTION_ENTER_MENU) goto InteractiveMenu;
    }

InteractiveMenu:
    {
        BootMenu_Action_t action = BootMenu_Interactive(&g_menu_ctx);
        if (action == BOOTMENU_ACTION_JUMP_APP) goto Check;
        else if (action == BOOTMENU_ACTION_ENTER_OTA)
        {
            if (OtaDownload_Execute(&g_ym_ctx, &g_ota_ctx, &param) == OTADL_STATUS_OK)
                goto Jump;
            else goto err;
        }
        else if (action == BOOTMENU_ACTION_ENTER_LORA_OTA)
        {
#if(configLORA)
            bootLoRa_ExitATMode();
#endif
            if (OtaDownload_Execute(&g_ym_ctx, &g_ota_ctx, &param) == OTADL_STATUS_OK)
                goto Jump;
            else goto err;
        }
        else if (action == BOOTMENU_ACTION_PRINT_OTA_PARAMS)
        {
            OTA_Param_t ota_param;
            if (bootOTA_ReadParamOTA(&g_ota_ctx, &ota_param) == 0) {
                printf("----------------------------------------\r\n");
                printf("   OTA Parameters\r\n");
                printf("  Magic: 0x%08lX  Part: %c  Ver: %lu\r\n",
                       ota_param.magic_flag,
                       (ota_param.active_partition == 0) ? 'A' : 'B',
                       ota_param.current_version);
                printf("----------------------------------------\r\n");
            }
            goto InteractiveMenu;
        }
        else goto err;
    }

Check:
    if (bootOTA_ReadParamOTA(&g_ota_ctx, &param) != 0) goto err;
    uint32_t active_addr = bootOTA_GetActivePartitionAddr(&param);
    if (param.magic_flag == configOTA_VALID_MAGIC &&
        CalcCRC16((uint8_t *)active_addr, param.app_size) == (uint16_t)param.app_crc)
        goto Jump;
    goto err;

Jump:
    printf("Ahh~I'm dead...~_~\r\n");
    Bootloader_JumpToApp(bootOTA_GetActivePartitionAddr(&param));

err:
    for(;;);
}
```

> `InteractiveMenu`、`Check`、`Jump`、`err` 标签段与硬件无关，可直接从原项目 `main.c` 完整复制。

### 9.5 修改 bootloader.c 中的 RAM 范围

**文件**: `Bootloader/component/bootloader/bootloader.c`

唯一需要改动的一行——SP 合法性检查：

```c
// F411 (128KB RAM):        0x20000000 ~ 0x20020000
// F103C8T6 (20KB RAM):     0x20000000 ~ 0x20005000
// F103RCT6 (48KB RAM):     0x20000000 ~ 0x2000C000
// 通用公式:                0x20000000 ~ (0x20000000 + RAM_SIZE)
if (app_sp >= 0x20000000 && app_sp <= (0x20000000 + <RAM_SIZE>))
```

---

## 10. 第九步: 修改 CMakeLists.txt

CubeMX 生成的 `CMakeLists.txt` 只包含它自己生成的文件。需要把 BootLoader 的源码和编译选项加进去。以下六段代码，追加到 CubeMX 生成的 CMakeLists.txt **末尾**（`project()` 之后）。

### 10.1 添加源文件

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Bootloader/service/ota/ota.c
    Bootloader/service/ymodem/ymodem.c
    Bootloader/service/sign_verify/sign_verify.c
    Bootloader/service/diff_update/diff_update.c
    Bootloader/service/menu/bootloader_menu.c
    Bootloader/component/bootloader/bootloader.c
    Bootloader/component/ota_download/ota_download.c
    Bootloader/port/janpatch/janpatch_port.c
    Bootloader/port/driver/uart/uart_dma_ring.c
    Bootloader/port/driver/flasher/flasher.c
    Bootloader/lib/hydrogen/hydrogen.c
    Bootloader/hardware/lora/lora.c
)
```

### 10.2 添加头文件路径

```cmake
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Bootloader
    Bootloader/service/ota
    Bootloader/service/ymodem
    Bootloader/service/sign_verify
    Bootloader/service/diff_update
    Bootloader/service/menu
    Bootloader/component/bootloader
    Bootloader/component/ota_download
    Bootloader/port/janpatch
    Bootloader/port/driver/flasher
    Bootloader/port/driver/uart
    Bootloader/lib/hydrogen
    Bootloader/lib/hydrogen/impl
    Bootloader/lib/hydrogen/impl/gimli-core
    Bootloader/lib/hydrogen/impl/random
    Bootloader/lib/janpatch
    Bootloader/hardware/lora
)
```

### 10.3 添加编译选项

```cmake
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
)
```

> CubeMX 已自动加了 `-mcpu`、`-mthumb`、`-mfloat-abi`、`-mfpu` 等 CPU 相关选项，不需要重复。

### 10.4 添加链接选项

```cmake
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Os
    -Wl,--gc-sections
)
```

### 10.5 添加生成 .bin 的后处理命令

```cmake
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating .bin file..."
)
```

### 10.6 去除 libc 链接依赖（可选，减小体积）

```cmake
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
```

---

## 11. 第十步: 各系列差异速查

| | F4→F4 | F4→F1 | F4→G0 | F4→G4 | F4→L4 |
|---|---|---|---|---|---|
| **链接脚本** | 改 Flash LENGTH | 改 Flash LENGTH | 改 Flash LENGTH | 改 Flash LENGTH | 改 Flash LENGTH |
| **configBootloader** | 改分区地址 | 改分区地址+使用页号 | 改分区地址+使用页号 | 改分区地址+使用页号 | 改分区地址+使用页号 |
| **flasher.c** | 免改 | 半字编程+页擦除 | 双字编程+页擦除 | 双字编程+页擦除 | 双字编程+页擦除 |
| **uart_dma_ring** | 免改 | 改 DMA 实例名 | 改 DMA 实例名 | 改 DMA 实例名 | 改 DMA 实例名 |
| **lora 驱动 (同模块)** | 免改 | 免改 | 免改 | 免改 | 免改 |
| **bootloader.c** | 改 RAM 范围 | 改 RAM 范围 | 改 RAM 范围 | 改 RAM 范围 | 改 RAM 范围 |
| **main.c** | 改 GPIO 宏名 | 改 GPIO 宏名 | 改 GPIO 宏名 | 改 GPIO 宏名 | 改 GPIO 宏名 |
| **CMake 编译选项** | CubeMX 已处理 | CubeMX 已处理 | CubeMX 已处理 | CubeMX 已处理 | CubeMX 已处理 |

> **F4→F4 同系列**: 只需改地址和 RAM 范围，半小时完成。
> **跨系列**: 额外工作集中在 `flasher.c` 的 Flash 编程方式适配。

---

## 12. 第十一步: 验证测试

按以下顺序逐项验证：

```
□ 1. 编译通过 (0 Error)
□ 2. 烧录，串口输出启动横幅
□ 3. 倒计时计数正常 (SysTick 正常)
□ 4. GPIO 拉低可触发 OTA 模式
□ 5. 交互菜单 5 个选项正常
□ 6. Flash 读写正常 (选项 4 查看 OTA 参数)
□ 7. 有线 Ymodem OTA 升级成功
□ 8. App 跳转后正常运行
□ 9. A/B 分区来回切换正常
□ 10. 签名验证: 合法通过 / 非法拒绝
□ 11. 防回滚: 低版本被拦截
□ 12. 差量更新 (如支持)
□ 13. LoRa 无线 OTA (如支持)
```

---

## 13. 常见问题排查

### Q1: 编译体积超过 BootLoader 空间

- 确认 `-Os` + `-Wl,--gc-sections` 已生效
- 确认链接脚本 `/DISCARD/` 段存在
- 关闭非必要功能: `configLORA = No`, `configUSE_FOOTER = No`
- 减半各缓冲大小

### Q2: 跳转 App 后 HardFault

- App 链接脚本起始地址 = `configBootloader.h` 分区地址？
- `CloseAllPeripheral()` 的 DMA 实例名已更新？DMA 反初始化排在 GPIO 之前？
- `SCB->VTOR` 已设置？
- App 向量表首字 (SP) 落在有效 RAM？次字 bit0 = 1 (Thumb)？

### Q3: Flash 写入后校验失败

- 编程宽度匹配？F1 必须 16-bit，G0/G4/L4 必须 64-bit
- Flash 写入前已解锁？写入后等待了 BSY 清零？
- 写入地址合法？

### Q4: Ymodem 反复超时

- PC 发送端波特率 = MCU 配置波特率？
- `configYM_BYTE_TIMEOUT_MS` / `configYM_PACKET_TIMEOUT_MS` 适当增大
- DMA 环形缓冲读写指针正常？

### Q5: DMA 丢数据导致 CRC 错误

- DMA 确认为 Circular Mode？
- `configRX_BUF_SIZE` ≥ 2048（覆盖整个 Ymodem 包）？
- 读写指针的回绕逻辑正确？

### Q6: Flash 擦/写无效果

- KEY1/KEY2 序列正确？Option Bytes 有写保护？EOP 等标志未清除？

### Q7: LoRa 模块初始化失败（死循环在 bootLoRa_Init 重试）

- MD0 / AUX 引脚在 CubeMX User Label 中是否正确命名？
- 模块上电完成？（AUX 初始电平是否与预期一致）
- USART 与模块的通信正常？（先通过有线 UART 确认串口工作）
- `configLORA_AT_TIMEOUT_MS` 是否太短？尝试增大到 10000
- 使用其他 AT 模块时：AT 指令格式是否匹配？响应 "OK" 的格式是否正确？
- 使用 SPI 模块时：SPI 时序是否匹配？寄存器地址是否对应？

### Q8: LoRa OTA 传输数据经常出错

- 适当增大 LoRa 的 Ymodem 超时: `configYM_BYTE_TIMEOUT_MS` 和 `configYM_PACKET_TIMEOUT_MS`
- 降低 LoRa 空中速率以提高可靠性
- 检查 `configLORA_AT_PARSER_WARMUP_MS` 是否足够（某些模块固件需要更长预热时间）

---

> **文档版本**: 1.0

> **最后更新**: 2026-06-10

> **参考项目**: STM32-LoraOTA-BootLoader

> **相关文档**: [技术实现文档 (README.md)](README.md)
