// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock[13];
  struct buf buf[NBUF];
  struct spinlock wholeLock;
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf bufBucket[13];
} bcache;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.wholeLock, "bcacheWhole");
  // 初始化bucket头和lock
  for(int i=0; i<13; i++){
    initlock(&bcache.lock[i], "bcacheLock");
    bcache.bufBucket[i].prev = bcache.bufBucket + i;
    bcache.bufBucket[i].next = bcache.bufBucket + i;
  }

  // 链接unusedBucket[bucketCode]链表，每次向头节点后面插入，修改4个指针
  int i=0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    int bucketCode = i % 13;
    b->blockno = i;
    b->next = bcache.bufBucket[bucketCode].next;
    b->prev = &bcache.bufBucket[bucketCode];
    initsleeplock(&b->lock, "buffer");
    bcache.bufBucket[bucketCode].next->prev = b;
    bcache.bufBucket[bucketCode].next = b;

    b->lastUsedTick = ticks;

    i++;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucketCode = blockno % 13;
  acquire(&bcache.lock[bucketCode]);
  // Is the block already cached?
  for(b = bcache.bufBucket[bucketCode].next; b != &bcache.bufBucket[bucketCode]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[bucketCode]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[bucketCode]);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  
  acquire(&bcache.wholeLock);

  b = 0;
  uint oldestUsedTick = 0xffffffff;

  // 在整个buf里找空，最早未被使用的buf，每次先获取那个bucket的lock，然后判断是不是最小的，是最小的就保持lock，否则释放lock，找到更小的就释放次小的lock，过程中要始终判断使得否是当前的lock
  for(int i = 0; i < 13; i++) {
    acquire(&bcache.lock[i]);
    char updated = 0;
    for(struct buf *bi = bcache.bufBucket[i].next; bi != &bcache.bufBucket[i]; bi = bi->next) {
      if(bi->refcnt == 0 && bi->lastUsedTick < oldestUsedTick) {
        if(b && b->blockno % 13!= i) release(&bcache.lock[b->blockno % 13]);
        oldestUsedTick = bi->lastUsedTick;
        b = bi;
        updated = 1;
      }
    }
    if(!updated) release(&bcache.lock[i]);
  }

  if(b == 0)
    panic("bget: no buffers");
  
  int oldBucketCode = b->blockno % 13;

  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  // 把b从原来的bucket中移除
  if (oldBucketCode != bucketCode) {
    b->prev->next = b->next;
    b->next->prev = b->prev;
  }
  release(&bcache.lock[oldBucketCode]);
  
  if(oldBucketCode != bucketCode) {
    // 把b加入现在的bucket
    acquire(&bcache.lock[bucketCode]);

    b->next = bcache.bufBucket[bucketCode].next;
    b->prev = &bcache.bufBucket[bucketCode];
    bcache.bufBucket[bucketCode].next->prev = b;
    bcache.bufBucket[bucketCode].next = b;

    release(&bcache.lock[bucketCode]);
  }
  release(&bcache.wholeLock);

  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucketCode = b->blockno % 13;
  acquire(&bcache.lock[bucketCode]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 记录下最后访问的时间
    b->lastUsedTick = ticks;
  }
  release(&bcache.lock[bucketCode]);
}

void
bpin(struct buf *b) {
  int bucketCode = b->blockno % 13;
  acquire(&bcache.lock[bucketCode]);
  b->refcnt++;
  release(&bcache.lock[bucketCode]);
}

void
bunpin(struct buf *b) {
  int bucketCode = b->blockno % 13;
  acquire(&bcache.lock[bucketCode]);
  b->refcnt--;
  release(&bcache.lock[bucketCode]);
}


