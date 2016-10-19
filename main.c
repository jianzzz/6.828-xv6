#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  //根据entry.S里面对栈的设置，可知栈顶位置在end之前。（存疑：根据objdump -h kernel结果和kernel.asm，entry.S里stack的位置并不在.data节内，为什么？）
  //将[end，4M](end先4K对齐)范围free为kmem，此时未开启分页，使用的是硬编码的4m地址映射。
  //kmem锁初始化，处于解锁状态，占有kmem锁的CPU数目为0
  //end: first address after kernel loaded from ELF file
  kinit1(end, P2V(4*1024*1024)); // phys page allocator // see in kalloc.c
  //为scheduler进程创建内核页目录，根据kmap设定将所有涉及范围内的内核空间虚拟地址
  //(从KERNBASE开始,把IO空间、内核镜像等都建立映射，这些空间全都是用户空间可见的，位于2GB以上)按页大小映射到物理地址上，
  //实际上是创建二级页表，并在二级页表项上存储物理地址。将页目录地址存储到cr3中。
  //页目录项所存页表的权限是用户可读写
  //二级页表项所存物理页的权限按照kmap设定
  kvmalloc();      // kernel page table //see in vm.c
  //
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  //更新当前cpu
  seginit();       // segment descriptors // see in vm.c
  //
  cprintf("\ncpu%d: starting xv6\n\n", cpunum());
  //
  picinit();       // another interrupt controller
  //
  ioapicinit();    // another interrupt controller
  //
  consoleinit();   // console hardware
  //
  uartinit();      // serial port
  //进程表锁初始化，设置进程表处于解锁状态，占有进程表锁的CPU数目为0
  pinit();         // process table //see in proc.c
  //建立正常的中断/陷阱门描述符,中断处理锁初始化，处于解锁状态，占有中断处理锁的CPU数目为0
  tvinit();        // trap vectors //see in trap.c
  //
  binit();         // buffer cache
  //
  fileinit();      // file table
  //
  ideinit();       // disk
  //
  if(!ismp)
    timerinit();   // uniprocessor timer
  //
  startothers();   // start other processors
  //将[4M,PHYSTOP]范围free为kmem，此时已经开启分页。
  //设置计数表示kmem锁处于使用状态
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers() // see in kalloc.c

  //1.
  //在进程表中寻找slot，成功的话更改进程状态embryo和pid，并初始化进程的内核栈
  //2.
  //为进程创建内核页目录，根据kmap设定将所有涉及范围内的内核空间虚拟地址
  //(从KERNBASE开始,把IO空间、内核镜像等都建立映射，这些空间全都是用户空间可见的，位于2GB以上)按页大小映射到物理地址上，
  //实际上是创建二级页表，并在二级页表项上存储物理地址。
  //每个进程都会新建页目录和二级页表
  //页目录项所存页表的权限是用户可读写
  //二级页表项所存物理页的权限按照kmap设定
  //3.
  //从kmem上分配一页的物理空间给进程，在新建进程的页目录上映射[0,PGSIZE]的虚拟地址到分配的物理地址上，将指向内容复制到物理内存上
  //页目录项所存页表的权限是用户可读写
  //二级页表项所存物理页的权限为用户可读写
  //4.
  //设置进程的trapframe
  //设置进程状态runnable
  userinit();      // first user process  //see in proc.c 
  //idtinit()加载中断描述符表寄存器
  //scheduler()无限循环寻找进程状态为runnable的进程
  //对于每次循环：
      //开中断运行当前进程
      //找到可执行的进程后，更新全局变量proc为当前被选中的进程
      //设置cpu环境后，加载选中进程的页目录地址到cr3,切换为进程的页目录
      //更改进程状态为running
      //CPU调度
      //加载内核的页目录地址到cr3,切换为内核的页目录
  mpmain();        // finish this processor's setup
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

// Common CPU setup code.
//idtinit()加载中断描述符表寄存器
//scheduler()无限循环寻找进程状态为runnable的进程
//对于每次循环：
    //开中断运行当前进程
    //找到可执行的进程后，更新全局变量proc为当前被选中的进程
    //设置cpu环境后，加载选中进程的页目录地址到cr3,切换为进程的页目录
    //更改进程状态为running
    //CPU调度
    //加载内核的页目录地址到cr3,切换为内核的页目录
static void
mpmain(void)
{
  cprintf("cpu%d: starting\n", cpunum());
  //加载中断描述符表寄存器
  idtinit();       // load idt register //see in trap.c
  xchg(&cpu->started, 1); // tell startothers() we're up //see in x86.h
  scheduler();     // start running processes //see in proc.c
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == cpus+cpunum())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void**)(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

    lapicstartap(c->apicid, V2P(code));

    // wait for cpu to finish mpmain()
    while(c->started == 0)
      ;
  }
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
