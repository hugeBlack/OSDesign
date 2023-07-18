#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;
struct spinlock e1000_lock2;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");
  initlock(&e1000_lock2, "e1000_2");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  // 发送mbuf的步骤：
  // 1. 根据E1000_TDT寄存器可以获取下一个可以放置mbuf的位置，检测器rt_desc中E1000_TXD_STAT_DD的寄存器，为1说明已经发送了mbuf，可以释放内存，为0说明还没有发送且环满了，应报错
  // 2. 创建tx_desc,设置指令为EOP（最后一个descriptor），RS（回报传输状态，这样才能读到E1000_TXD_STAT_DD）
  // 3. 把帧的地址放在tx环中，放置完后E1000_TDT前进一格
  acquire(&e1000_lock);
  uint32 nextDescIndex = regs[E1000_TDT];
  //检查是否已经传输完毕
  if(!(tx_ring[nextDescIndex].status & E1000_TXD_STAT_DD)){
    release(&e1000_lock);
    return -1;
  }
  //释放内存
  if(tx_mbufs[nextDescIndex]) mbuffree(tx_mbufs[nextDescIndex]);
  //清空desc
  memset(tx_ring+nextDescIndex,0,sizeof(struct tx_desc));
  //设置desc的属性
  tx_ring[nextDescIndex].addr = (uint64)m->head;
  tx_ring[nextDescIndex].length = (uint16)m->len;
  tx_ring[nextDescIndex].cmd = (E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);

  tx_mbufs[nextDescIndex] = m;
  //TDT指向下一个空位
  regs[E1000_TDT] = (nextDescIndex + 1) % TX_RING_SIZE;
  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  // 可能会有很多数据包，要用while死循环读取
  // E1000_RDT的下一个rx_desc即为要接收的数据包
  // E1000_RXD_STAT_DD标志了这个数据包是否是新收到的
  // 不是就说明没有新的数据包，直接返回，否则修改mbuf的长度、指针等属性，用net_rx()返回
  // 然后分配一个新的mbuf
  // 最后更新E1000_RDT
  acquire(&e1000_lock2);

  uint32 nextDescIndex;
  while(1){
    // 获取下一个数据包的索引
    nextDescIndex = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    // 数据包已经被读取了就break
    if(!(rx_ring[nextDescIndex].status & E1000_RXD_STAT_DD)) break;
    // 设置一下mbuf的长度，并调用net_rx读取mbuf
    rx_mbufs[nextDescIndex]->len = rx_ring[nextDescIndex].length;
    net_rx(rx_mbufs[nextDescIndex]);
    // 分配一个新的mbuf
    rx_mbufs[nextDescIndex] = mbufalloc(0);
    rx_ring[nextDescIndex].addr = (uint64)rx_mbufs[nextDescIndex]->head;
    rx_ring[nextDescIndex].status = 0;
    //RDT指向下一个空位
    regs[E1000_RDT] = nextDescIndex;
  }

  release(&e1000_lock2);
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
