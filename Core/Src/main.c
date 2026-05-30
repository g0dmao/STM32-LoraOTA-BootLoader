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
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

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
  uint8_t  new_active = 0;



/*-----------------------------------------------*/
// 开机校验：读取 OTA 参数，校验活跃分区 CRC

  if (bootOTA_ReadParamOTA(&ota_ctx, &param) != 0)
  {
    goto err;
  }

  uint32_t active_addr = bootOTA_GetActivePartitionAddr(&param);

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
      uint32_t new_app_crc = CalcCRC16((uint8_t*)write_addr, ym_ctx.file_size);
      new_active = (param.active_partition == 0) ? 1 : 0;
      bootOTA_SaveParamOTA(&ota_ctx, ym_ctx.file_size, new_app_crc, new_active);
      param.active_partition = new_active;
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
    HAL_Delay(1000);
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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
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

    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);

    return len; // 必须返回实际发送的字节数
}

void CloseAllPeripheral(void)
{
  HAL_UART_DeInit(&huart1);
  HAL_UART_MspDeInit(&huart1);
}

void BootloaderInit(void)
{
  ym_ctx.read_byte_cb = bootUART_ReadByte;
  ym_ctx.send_byte_cb = bootUART_SendByte;
  ym_ctx.get_tick_cb  = HAL_GetTick;

  ota_ctx.param_address = configPARAM_ADDRESS;
  ota_ctx.param_sector = configPARAM_SECTOR;
  ota_ctx.param_sector_num = configPARAM_SECTOR_NUMBER;
  ota_ctx.read_cb = bootFlasher_ReadData;
  ota_ctx.write_cb = bootFlasher_WriteByte;
  ota_ctx.erase_cb = bootFlasher_EraseSectors;

  // 初始化用于上位机传输文件的串口，而不是打印调试信息的串口
  bootUART_RegisterTransmitPort(&huart1);

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
