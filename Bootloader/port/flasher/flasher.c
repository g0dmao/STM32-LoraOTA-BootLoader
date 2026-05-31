#include "flasher.h"
#include <string.h>

#if(!(configUSE_CUSTOM_FLASH))

#include "stm32f4xx.h"

#define FLASH_KEY1  0x45670123U
#define FLASH_KEY2  0xCDEF89ABU

/* ---- 静态辅助函数 ---- */

/**
 * @brief  解锁 Flash 控制寄存器
 */
static void flash_unlock(void)
{
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
}

/**
 * @brief  上锁 Flash 控制寄存器
 */
static void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

/**
 * @brief  清除 Flash 状态寄存器全部错误/完成标志（写 1 清零）
 */
static void flash_clear_flags(void)
{
    FLASH->SR = (FLASH_SR_EOP   | FLASH_SR_OPERR  | FLASH_SR_WRPERR |
                 FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR |
                 FLASH_SR_RDERR);
}

/**
 * @brief  等待 Flash 忙标志清除，并检查错误标志
 * @retval 0: 成功; -1: 出错
 */
static int8_t flash_wait_done(void)
{
    while (FLASH->SR & FLASH_SR_BSY);

    if (FLASH->SR & (FLASH_SR_WRPERR | FLASH_SR_PGAERR |
                     FLASH_SR_PGPERR | FLASH_SR_PGSERR))
    {
        return -1;
    }

    if (FLASH->SR & FLASH_SR_EOP)
    {
        FLASH->SR = FLASH_SR_EOP;  /* 写 1 清零 */
    }

    return 0;
}


/* ---- 对外接口 ---- */

/**
 * @brief  擦除 App 所在的 Flash 扇区
 * @param  sector:        起始扇区号 (0-11)
 * @param  sector_number: 连续擦除的扇区个数
 * @retval 0: 成功; -1: 失败
 */
int8_t bootFlasher_EraseSectors(int sector, int sector_number)
{
    flash_unlock();
    flash_clear_flags();

    for (int i = 0; i < sector_number; i++)
    {
        int current_sector = sector + i;

        /* 配置扇区擦除：SER + SNB + PSIZE(x16) + 启动 */
        FLASH->CR &= ~(FLASH_CR_SNB | FLASH_CR_PSIZE);
        FLASH->CR |= FLASH_CR_SER
                  |  (current_sector << FLASH_CR_SNB_Pos)
                  |  FLASH_CR_PSIZE_1;

        FLASH->CR |= FLASH_CR_STRT;

        if (flash_wait_done() != 0)
        {
            FLASH->CR &= ~FLASH_CR_SER;
            flash_lock();
            return -1;
        }
    }

    FLASH->CR &= ~FLASH_CR_SER;
    flash_lock();
    return 0;
}


/**
 * @brief  向目标 Flash 地址写入连续数据（逐字节写入）
 * @param  address: 写入起始物理地址
 * @param  data:    数据缓冲区指针
 * @param  length:  写入字节数
 * @retval 0: 成功; -1: 失败
 */
int8_t bootFlasher_WriteByte(uint32_t address, uint8_t *data, uint16_t length)
{
    flash_unlock();
    flash_clear_flags();

    /* 启用编程模式，PSIZE 保持默认 00 = x8 (byte) */
    FLASH->CR |= FLASH_CR_PG;

    for (uint16_t i = 0; i < length; i++)
    {
        *(__IO uint8_t *)(address + i) = data[i];

        if (flash_wait_done() != 0)
        {
            FLASH->CR &= ~FLASH_CR_PG;
            flash_lock();
            return -1;
        }
    }

    FLASH->CR &= ~FLASH_CR_PG;
    flash_lock();
    return 0;
}


/**
 * @brief 从 Flash 地址读取数据（Flash 内存映射，直接 memcpy）
 */
int8_t bootFlasher_ReadData(uint32_t address, uint8_t *data, uint16_t length)
{
    memcpy(data, (void*)address, length);
    return 0;
}

#endif
