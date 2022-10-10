#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int n;

  if (argint(0, &n) < 0) {
    return -1;
  }

  myproc() -> tracemask = n;
  return 0;
}

uint64
sys_sysinfo(void)
{
  uint64 addr;
  struct proc *p = myproc();

  if (argaddr(0, &addr) < 0) {
    return -1;
  }

  // 声明sysinfo
  struct sysinfo info;
  // 需要实现三个函数分别计算剩余内存空间、空闲进程、可用文件描述符
  info.freemem  = get_free_mem();             // 内存空间
  info.nproc    = get_unused_proc_num();      // 空闲进程
  info.freefd   = get_free_fd();              // 可用文件描述符

  // copyout 参数: 进程页表， 用户态目标地址， 数据源地址，数据大小
  // 返回值： 数据大小
  if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}