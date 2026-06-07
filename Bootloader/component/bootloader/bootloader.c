#include "bootloader.h"
#include "main.h"

extern void CloseAllPeripheral(void);

__attribute__((naked, noreturn)) static void bootJump_Execute(uint32_t app_sp, uint32_t app_pc)
{
    __asm volatile (
        "msr msp, r0\n"      // 将第一个参数 (app_sp) 写入主堆栈指针 MSP
        "bx r1\n"            // 跳转到第二个参数 (app_pc) 指向的地址
    );
}

void Bootloader_JumpToApp(uint32_t app_address)
{

  // 读取 App 的栈顶指针
  uint32_t app_sp = *(__IO uint32_t*)app_address;

  // 读取 App 的复位中断服务函数地址
  uint32_t app_pc = *(__IO uint32_t*)(app_address + 4);

  // 检查栈顶指针是否合法
  // F411 RAM 起始于 0x20000000，大小 128KB (0x20000000 ~ 0x2001FFFF)
  // 实测空栈栈顶指针为 0x2001FFFF+1
  if (app_sp >= 0x20000000 && app_sp <= 0x20020000)
  {
    __disable_irq();  // 全局关闭中断

    // 彻底关闭各种外设
    CloseAllPeripheral();

    // 清理NVIC
    for (int i = 0; i < 8; i++)
    {
        // 关闭所有中断使能
        NVIC->ICER[i] = 0xFFFFFFFF; // 中断清除使能寄存器，写1清除使能
        // 清除所有挂起标志
        NVIC->ICPR[i] = 0xFFFFFFFF; // 中断清除挂起标志寄存器，同上
    }

    // 确保 CPU 处于特权级线程模式，并且使用的是主堆栈 (MSP)
    __set_CONTROL(0);

    // === 交接 ===

    // 设置中断向量表基地址；设置主堆栈指针为 App 的 SP
    SCB->VTOR = app_address;

    bootJump_Execute(app_sp, app_pc);

  }else
  {
      // 栈顶指针不合法，说明 App 区没有有效的代码，或者被破坏了
      while(1);
  }

}







