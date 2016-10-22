#include "mmu.h"

  # vectors.S sends all traps here.
.globl alltraps
alltraps:
  # Build trap frame. //将用户进程的寄存器保存
  pushl %ds
  pushl %es
  pushl %fs
  pushl %gs
  pushal
  
  # Set up data and per-cpu segments.
  #此处代表由用户态切换到内核态？此时已经由p->tf->esp指向的进程用户栈切换到ssX:espX指定的栈中，
  #这里是p->kstack指向的进程内核栈，当前进程相关的寄存器保存到了进程内核栈中。
  movw $(SEG_KDATA<<3), %ax
  movw %ax, %ds
  movw %ax, %es
  movw $(SEG_KCPU<<3), %ax
  movw %ax, %fs
  movw %ax, %gs

  # Call trap(tf), where tf=%esp
  pushl %esp #push %esp的原因是将当前esp指向的tf结构基地址作为传递的参数
  call trap
  addl $4, %esp

  # Return falls through to trapret...
.globl trapret
trapret:
  #此处代表由内核态切换到用户态？主要是弹出保存在ssX:espX指定的栈中的进程相关的寄存器，返回执行进程代码。
  #第一次切换到用户态执行代码时，ssX:espX指定的栈中的进程相关的寄存器是在userinit函数中指定的。
  popal #弹出通用寄存器中的上下文环境
  popl %gs
  popl %fs
  popl %es
  popl %ds
  addl $0x8, %esp  # trapno and errcode
  iret
