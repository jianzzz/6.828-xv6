// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    //mknod在文件系统中创建一个没有任何内容的文件。相反，这个文件的原数据标识表明它是一个设备文件，
    //并且记录了主、副设备号（mknod的两个参数），这是内核设备的唯一标识。
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  //使0、1、2文件描述符均指向console设备文件
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    printf(1, "init: starting sh\n");
    pid = fork();//see in proc.c
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      exec("sh", argv);//创建子进程，子进程调用exec执行sh，不会再返回
      printf(1, "init: exec sh failed\n");
      exit();
    }
    //注意到子进程exec并没有改变父进程，所有子进程结束的时候仍然可以被父进程处理
    //init进程处理僵尸程序
    while((wpid=wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");
  }
}
