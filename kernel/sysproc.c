#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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


#ifdef LAB_PGTBL

pte_t* getPteL0(pagetable_t pgtbl, uint64 nowVPageAddr){
    //先通过三级页表把L0的pte找出来，需要对原始的pte进行修改，所以返回地址
    pte_t pteL2 = pgtbl[nowVPageAddr >> 30 & 0x1ff];
    if(!(pteL2 & PTE_V)) return 0;
    pagetable_t paL2= (pagetable_t) PTE2PA(pteL2);
    pte_t pteL1 = paL2[nowVPageAddr >> 21 & 0x1ff];
    if(!(pteL1 & PTE_V)) return 0;
    pagetable_t paL1= (pagetable_t) PTE2PA(pteL1);
    pte_t *pteL0Ptr = paL1 + (nowVPageAddr >> 12 & 0x1ff);
    if(!(*pteL0Ptr & PTE_V)) return 0;
    return pteL0Ptr;
}

int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  //三个参数，第一个页面的首虚拟地址、要检查的页面数量、储存结果的buffer
  uint64 pgtblAddr;
  int pageCount;
  uint64 bufferAddr;
  if(argaddr(0, &pgtblAddr) < 0)
    return -1;
  if(argint(1, &pageCount) < 0)
    return -1;
  if(argaddr(2, &bufferAddr) < 0)
    return -1;
  if(pageCount>1024 || pageCount < 0){
    return -1;
  }

  char result[128];
  pagetable_t pgtbl = myproc()->pagetable;
  for(int i=0;i<pageCount;i++){
    uint64 nowVPageAddr = pgtblAddr + (i << 12);
    pte_t* pteL0Ptr = getPteL0(pgtbl, nowVPageAddr);
    //检查该页面是否被使用
    if(pteL0Ptr){
      //找到对应的页表的pte，置为PTE_A
      if(*pteL0Ptr & PTE_A){
        //访问过了，置为1，用或
        result[i>>3] = result[i>>3] | (1<<(i&7));
      }else{
        //没访问过，置为0，用与
        result[i>>3] = result[i>>3] & ~(1<<(i&7));
      }
      //置PTE_A为0
      *pteL0Ptr &= ~PTE_A;
      printf("%p %p\n", getPteL0(pgtbl,nowVPageAddr), *pteL0Ptr);
    }else{
      result[i>>3] = result[i>>3] & ~(1<<(i&7));
    }
  }
  //计算下需要copy的字节数
  int length = pageCount >> 3;
  if(!(pageCount&7)) length++;
  copyout(pgtbl, bufferAddr, result, length);

  return 0;
}
#endif

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
