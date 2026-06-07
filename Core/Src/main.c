/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
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
#include "lora.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

YM_InfoBlock_t   g_ym_ctx;
OTA_Context_t    g_ota_ctx;
LoRa_Callback_t  g_lora_cb;
BootMenu_Context_t g_menu_ctx;

/* extern from Core/Src/stm32f4xx_it.c */
extern volatile uint32_t g_sys_tick;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void CloseAllPeripheral(void);
static void BootloaderInit(void);
static uint32_t GetTick(void);
static void LoRa_SetMD0(uint8_t level);
static uint8_t LoRa_ReadAUX(void);
static uint8_t ReadOtaPin(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  /* System interrupt init*/
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  BootloaderInit();
  BootMenu_PrintBanner();

  OTA_Param_t param;


/*-----------------------------------------------*/
// 进入OTA下载的触发逻辑
  {
    uint32_t start_time = GetTick();
    for (;;)
    {
        BootMenu_Action_t action = BootMenu_Poll(&g_menu_ctx, GetTick() - start_time);

        if (action == BOOTMENU_ACTION_ENTER_OTA)
        {
            if (OtaDownload_Execute(&g_ym_ctx, &g_ota_ctx, &param) == OTADL_STATUS_OK)
            {
                goto Jump;
            }
            else
            {
                goto err;
            }
        }
        if (action == BOOTMENU_ACTION_ENTER_LORA_OTA)
        {
            bootLoRa_ExitATMode();   /* 确保 LoRa 在透传模式 */
            if (OtaDownload_Execute(&g_ym_ctx, &g_ota_ctx, &param) == OTADL_STATUS_OK)
            {
                goto Jump;
            }
            else
            {
                goto err;
            }
        }
        if (action == BOOTMENU_ACTION_JUMP_APP)
        {
            goto Check;
        }
        if (action == BOOTMENU_ACTION_ENTER_MENU)
        {
            goto InteractiveMenu;
        }
    }
  }
/*-----------------------------------------------*/


InteractiveMenu:
/*-----------------------------------------------*/
// InteractiveMenu: 交互菜单模式

  {
    BootMenu_Action_t action = BootMenu_Interactive(&g_menu_ctx);

    if (action == BOOTMENU_ACTION_JUMP_APP)
    {
        goto Check;
    }
    else if (action == BOOTMENU_ACTION_ENTER_OTA)
    {
        if (OtaDownload_Execute(&g_ym_ctx, &g_ota_ctx, &param) == OTADL_STATUS_OK)
        {
            goto Jump;
        }
        else
        {
            goto err;
        }
    }
    else if (action == BOOTMENU_ACTION_ENTER_LORA_OTA)
    {
        bootLoRa_ExitATMode();   /* 确保 LoRa 在透传模式 */
        if (OtaDownload_Execute(&g_ym_ctx, &g_ota_ctx, &param) == OTADL_STATUS_OK)
        {
            goto Jump;
        }
        else
        {
            goto err;
        }
    }
    else if (action == BOOTMENU_ACTION_PRINT_OTA_PARAMS)
    {
        OTA_Param_t ota_param;
        if (bootOTA_ReadParamOTA(&g_ota_ctx, &ota_param) == 0)
        {
            printf("----------------------------------------\r\n");
            printf("   OTA Parameters  (o..o)\r\n");
            printf("  -----------------------------------\r\n");
            printf("  Magic Flag:       0x%08lX", (unsigned long)ota_param.magic_flag);
            if (ota_param.magic_flag != configOTA_VALID_MAGIC)
            {
                printf(" (INVALID!)\r\n");
            }
            else
            {
                printf(" (VALID)\r\n");
            }
            printf("  App Size:         %lu bytes\r\n", ota_param.app_size);
            printf("  App CRC:          0x%04X\r\n", (uint16_t)ota_param.app_crc);
            printf("  Active Partition: %c (0x%08lX)\r\n",
                    (ota_param.active_partition == 0) ? 'A' : 'B',
                    (unsigned long)((ota_param.active_partition == 0) ? configPART_A_ADDRESS
                                                                      : configPART_B_ADDRESS));
            printf("  Current Version:  %lu\r\n", ota_param.current_version);
            printf("----------------------------------------\r\n");
        }
        else
        {
            printf("[!] Failed to read OTA parameters!\r\n");
        }
        goto InteractiveMenu;
    }
    else
    {
      goto err;
    }


  }
/*-----------------------------------------------*/



Check:
/*-----------------------------------------------*/
// 开机校验：读取 OTA 参数，校验活跃分区 CRC

  {
    if (bootOTA_ReadParamOTA(&g_ota_ctx, &param) != 0)
    {
      goto err;
    }

    uint32_t active_addr = bootOTA_GetActivePartitionAddr(&param);

    if (param.magic_flag == configOTA_VALID_MAGIC &&
        CalcCRC16((uint8_t*)active_addr, param.app_size) == (uint16_t)param.app_crc)
    {
      goto Jump;
    }
  }
/*-----------------------------------------------*/


Jump:
/*-----------------------------------------------*/
// Jump: 跳转到活跃分区 App

  printf("Ahh~I'm dead...~_~\r\n");
  Bootloader_JumpToApp(bootOTA_GetActivePartitionAddr(&param));
/*-----------------------------------------------*/


err:
/*-----------------------------------------------*/
  for(;;);
/*-----------------------------------------------*/





  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_3)
  {
  }
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {

  }
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_12, 96, LL_RCC_PLLP_DIV_2);
  LL_RCC_PLL_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {

  }
  while (LL_PWR_IsActiveFlag_VOS() == 0)
  {
  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {

  }
  LL_Init1msTick(100000000);
  LL_SetSystemCoreClock(100000000);
  LL_RCC_SetTIMPrescaler(LL_RCC_TIM_PRESCALER_TWICE);
}

/* USER CODE BEGIN 4 */

/**
 * @brief 重写 GNU C 库的底层 write 函数
 *
 * @param file 文件描述符 (1 代表标准输出 stdout)
 * @param ptr 要发送的字符数组指针
 * @param len 要发送的长度
 */
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        while (!LL_USART_IsActiveFlag_TXE(USART1));
        LL_USART_TransmitData8(USART1, ptr[i]);
    }
    return len;
}

void CloseAllPeripheral(void)
{

  LL_USART_DeInit(USART1);
  LL_DMA_DeInit(DMA2,LL_DMA_STREAM_2);
  LL_GPIO_DeInit(GPIOA);

}

static uint32_t GetTick(void)
{
  return g_sys_tick;
}

/**
 * @brief  MD0 引脚控制（占位，由用户按实际硬件补全）
 * @param  level  1 = 高电平（AT 模式）; 0 = 低电平（数据模式）
 */
static void LoRa_SetMD0(uint8_t level)
{

  if (level)
      LL_GPIO_SetOutputPin(LoRa_MD0_GPIO_Port, LoRa_MD0_Pin);
  else
      LL_GPIO_ResetOutputPin(LoRa_MD0_GPIO_Port, LoRa_MD0_Pin);

}

/**
 * @brief  AUX 引脚读取（占位，由用户按实际硬件补全）
 * @retval 1: 高电平（忙）; 0: 低电平（空闲）
 */
static uint8_t LoRa_ReadAUX(void)
{

    return ((LL_GPIO_ReadInputPort(LoRa_AUX_GPIO_Port) & BOOT_Pin)? 1 : 0);

}

/**
 * @brief  OTA 触发引脚读取（供菜单服务层回调）
 * @retval 1: PA0 低电平（触发 OTA）; 0: 高电平（未触发）
 */
static uint8_t ReadOtaPin(void)
{
    return ((LL_GPIO_ReadInputPort(GPIOA) & BOOT_Pin) ? 0 : 1);
}

static void BootloaderInit(void)
{
  g_ym_ctx.read_byte_cb = bootUART_ReadByte;
  g_ym_ctx.send_byte_cb = bootUART_SendByte;
  g_ym_ctx.get_tick_cb  = GetTick;

  g_ota_ctx.read_cb   = bootFlasher_ReadData;
  g_ota_ctx.write_cb  = bootFlasher_WriteByte;
  g_ota_ctx.erase_cb  = bootFlasher_EraseSectors;
  g_ota_ctx.unlock_cb = bootFlasher_Unlock;
  g_ota_ctx.lock_cb   = bootFlasher_Lock;

  // 初始化用于上位机传输文件的串口，而不是打印调试信息的串口
  bootUART_RegisterTransmitPort();

#if(configLORA)
  g_lora_cb.read_byte_cb = bootUART_ReadByte;
  g_lora_cb.send_byte_cb = bootUART_SendByte;
  g_lora_cb.get_tick_cb  = GetTick;
  g_lora_cb.set_md0_cb   = LoRa_SetMD0;
  g_lora_cb.read_aux_cb  = LoRa_ReadAUX;

  int8_t retry = 3 + 1; /* 重试次数为3 */
  int ret;
  do {
    if(retry <= 0)
    {
      for(;;);
    }
    ret = bootLoRa_Init(&g_lora_cb);
    retry--;
  }while (ret != 0);


#endif

  g_menu_ctx.read_byte_cb    = bootUART_ReadByte;
  g_menu_ctx.read_ota_pin_cb = ReadOtaPin;

}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
