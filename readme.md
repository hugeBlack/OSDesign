# Lab10:mmap

## 思路

需求分为三个部分：设置映射、读写映射区域、撤销映射

在进程fork的时候需要给子进程复制一份vma，并使得文件的共享记数+1

进程退出时要调用munmap

### mmap

1. 进程pcb需要记录16个mva信息，虚拟地址分配可以从MAXVA>>1开始分配，

2. 每分配一个就让下一次分配的地址+=length，记录在pcb里

要注意的是共享只能以页面为单位，所以开始地址与页面对齐

### 页中断读写映射区域

读到mmap区域的时候：
1. 扫一遍vma表，找到对应的vma
2. 分配页面
3. 然后设置页表映射

### unmap

1. ummap的时候要找到对应的vma
2. 如果有share需要保存
3. unmap的同时要释放内存
4. 之后修改vma的信息（长度&头部），如果是全部unmap就清除vma，

iunlockput是解锁并释放inode指针的意思，这里只能unlock，解除占用可以用closefile，会将引用数减一，如果没有引用就自动关闭文件