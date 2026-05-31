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
#include "sign_verify.h"
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

YM_InfoBlock_t ym_ctx;
OTA_Context_t  ota_ctx;

/* extern from Core/Src/stm32f4xx_it.c */
extern volatile uint32_t g_sys_tick;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void CloseAllPeripheral(void);
void BootloaderInit(void);
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
  printf("------ welcome to GoDm@Bootloader! ------\r\n");

  OTA_Param_t param;
  uint32_t write_offset = 0;
  uint32_t write_addr   = 0;
  int      sector = 0, sector_num = 0;


/*-----------------------------------------------*/
// 开机校验：读取 OTA 参数，校验活跃分区 CRC

  if (bootOTA_ReadParamOTA(&ota_ctx, &param) != 0)
  {
    goto err;
  }

  uint32_t active_addr = bootOTA_GetActivePartitionAddr(&param);

  // 调试用，直接跳到下载循环
  goto loop_YM_ConnectAndErase;

  if (param.magic_flag == configOTA_VALID_MAGIC &&
      CalcCRC16((uint8_t*)active_addr, param.app_size) == (uint16_t)param.app_crc)
  {
    goto Jump;
  }
/*-----------------------------------------------*/


loop_YM_ConnectAndErase:
/*-----------------------------------------------*/
// loop_YM_ConnectAndErase:
// 尝试与 Ymodem 上位机建立连接，一旦连接成功，擦除【非活跃】分区

  for(;;)
  {
    if(bootYM_EstablishConnection(&ym_ctx) == YM_RETURN_CODE_OK)
    {
      bootOTA_GetInactivePartitionEraseInfo(&param, &sector, &sector_num);
      if(ota_ctx.erase_cb(sector, sector_num) != 0)
      {
        bootYM_Abort(&ym_ctx);
        goto err;
      }
      write_addr = bootOTA_GetInactivePartitionAddr(&param);
      goto loop_YM_ReceiveAndFlash;
    }
  }
/*-----------------------------------------------*/


loop_YM_ReceiveAndFlash:
/*-----------------------------------------------*/
// loop_YM_ReceiveAndFlash: 接收 Ymodem 数据包并写入【非活跃】分区

  for(;;)
  {
    int8_t ret = bootYM_AccepctOnePacket(&ym_ctx);

    if(ret == YM_RETURN_CODE_OK)
    {
      write_offset = ym_ctx.total_receive_byte - ym_ctx.packet_len;
      if(ota_ctx.write_cb(write_addr + write_offset, ym_ctx.packet_data, ym_ctx.packet_len) != 0)
      {
        bootYM_Abort(&ym_ctx);
        goto err;
      }
    }else if(ret == YM_RETURN_CODE_EOT)
    {
      uint32_t fw_bin_size = 0;
      FW_SignInfo_t sign_info;
      #if(configUSE_FOOTER)
      /* 1. 解析 Footer + 签名校验 */
      int8_t sig_ret = bootSIG_ParseAndVerify(write_addr, ym_ctx.file_size,
                                               &sign_info, &fw_bin_size);
      if (sig_ret != 0)
      {
        printf("SIG_ERR: %d\r\n", sig_ret);
        bootYM_Abort(&ym_ctx);
        goto err;
      }

      /* 2. 防回滚检查：新版本号必须 >= 当前版本号 */
      if (sign_info.version < param.current_version)
      {
        printf("ROLLBACK: v%lu < v%lu\r\n", sign_info.version, param.current_version);
        bootYM_Abort(&ym_ctx);
        goto err;
      }

      #else
        fw_bin_size = ym_ctx.file_size;
        sign_info.version = 0xFFFFFFFF;
      #endif

      /* 3. CRC 校验（仅固件本体，不含 Footer） */
      OTA_Param_t new_param = {
        .app_size   = fw_bin_size,
        .app_crc    = CalcCRC16((uint8_t*)write_addr, fw_bin_size),
        .active_partition = (uint8_t)((param.active_partition == 0) ? 1 : 0),
        .current_version  = sign_info.version,
        .reserved   = {0xFF, 0xFF, 0xFF}
      };

      bootOTA_SaveParamOTA(&ota_ctx, &new_param);

      param.active_partition = new_param.active_partition;
      param.current_version  = new_param.current_version;
      goto Jump;
    }else
    {
      bootYM_Abort(&ym_ctx);
      goto err;
    }
  }
/*-----------------------------------------------*/


Jump:
/*-----------------------------------------------*/
// Jump: 跳转到活跃分区 App

  int i = 3;
  while(i)
  {
    printf("jump after %d seconds\r\n", i);
    i--;
    LL_mDelay(1000);
  }
  printf("Byebye:)\r\n");
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
}

static uint32_t GetTick(void)
{
  return g_sys_tick;
}

void BootloaderInit(void)
{
  ym_ctx.read_byte_cb = bootUART_ReadByte;
  ym_ctx.send_byte_cb = bootUART_SendByte;
  ym_ctx.get_tick_cb  = GetTick;

  ota_ctx.read_cb = bootFlasher_ReadData;
  ota_ctx.write_cb = bootFlasher_WriteByte;
  ota_ctx.erase_cb = bootFlasher_EraseSectors;

  // 初始化用于上位机传输文件的串口，而不是打印调试信息的串口
  bootUART_RegisterTransmitPort();

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
