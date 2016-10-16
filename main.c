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
  //将[end，4M](end先4K对齐)范围free为kmem，此时未开启分页，使用的是硬编码的4m地址映射。
  //kmem锁初始化，处于解锁状态，占有kmem锁的CPU数目为0
  //end: first address after kernel loaded from ELF file
  kinit1(end, P2V(4*1024*1024)); // phys page allocator // see in kalloc.c
  //为scheduler进程创建内核页目录，根据kmap设定将所有涉及范围内的内核空间虚拟地址(从KERNBASE开始)按页大小映射到物理地址上，
  //实际上是创建二级页表，并在二级页表项上存储物理地址。将页目录地址存储到cr3中。
  //页目录项所存页表的权限是用户可读写
  kvmalloc();      // kernel page table //see in vm.c
  //
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  //
  seginit();       // segment descriptors
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
  //设置中断处理程序入口,中断处理锁初始化，处于解锁状态，占有中断处理锁的CPU数目为0
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
  //
  userinit();      // first user process  //see in proc.c
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
static void
mpmain(void)
{
  cprintf("cpu%d: starting\n", cpunum());
  idtinit();       // load idt register
  xchg(&cpu->started, 1); // tell startothers() we're up
  scheduler();     // start running processes
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
