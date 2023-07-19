// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  // 每个CPU都有一个lock与freeList
  struct spinlock lock;
  struct run *freelist;
  // 每个CPU拥有的空闲页数
  int freePageCount;
} kmem[NCPU];

void
kinit()
{
  for(int i=0;i<NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 优先把空闲页释到放当前的CPU
  push_off();
  int nowCpuId = cpuid();
  acquire(&kmem[nowCpuId].lock);
  r->next = kmem[nowCpuId].freelist;
  kmem[nowCpuId].freelist = r;
  // 记录空闲页数变化
  kmem[nowCpuId].freePageCount++;
  release(&kmem[nowCpuId].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  // 优先使用当前的CPU的空闲页
  push_off();
  int nowCpuId = cpuid();
  acquire(&kmem[nowCpuId].lock);
  // 检查是否还有空闲页
  r = kmem[nowCpuId].freelist;
  if(r){
    //有就分配
    kmem[nowCpuId].freelist = r->next;
    // 记录空闲页数变化
    kmem[nowCpuId].freePageCount--;
    release(&kmem[nowCpuId].lock);
  }else{
    release(&kmem[nowCpuId].lock);
    // 没有就从拥有最多空闲页的cpu偷一个
    int selectedCpuId = 0;
    acquire(&kmem[0].lock);
    for(int i=1;i<NCPU; i++){
      if(i==nowCpuId) continue;
      acquire(&kmem[i].lock);
      if(kmem[i].freePageCount>kmem[selectedCpuId].freePageCount){
        release(&kmem[selectedCpuId].lock);
        selectedCpuId = i;
      }else{
        release(&kmem[i].lock);
      }
    }
    r = kmem[selectedCpuId].freelist;
    if(r){
      //最多的CPU有就分配
      kmem[selectedCpuId].freelist = r->next;
      // 记录空闲页数变化
      kmem[selectedCpuId].freePageCount--;
    }
    release(&kmem[selectedCpuId].lock);
  }
  pop_off();
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
