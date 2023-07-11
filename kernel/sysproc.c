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
sys_trace(void){
  int mask;
  acquire(&tickslock);
  if (argint(0, &mask) < 0)
    return -1;

  struct proc *pro = myproc();
  pro->traceMask = mask;
  release(&tickslock);
  return 0;
}

uint64
sys_sysinfo(void){
  int ans = 0;
  uint64 infoAddr;
  acquire(&tickslock);
  if (argaddr(0, &infoAddr) < 0)
    return -1;

  struct sysinfo info;
  info.nproc = getRunningProcCount();
  info.freemem = getFreePageCount()*4096;
  //内核和进程寻址方式、空间不同，不能直接赋值，要用copyout
  struct proc *p = myproc();
  // 如果传入恶意地址，需要返回-1报错
  if(copyout(p->pagetable,infoAddr,(char*)&info,sizeof(info)) < 0)
    ans = -1;
  release(&tickslock);
  return ans;
}