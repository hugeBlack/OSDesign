# RISC-V assembly

### Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?

函数的前8个参数存在a0~a7寄存器，13存在a2中

### Where is the call to function f in the assembly code for main? Where is the call to g? 

在main的0x1c，编译器内联了f，但是没有调用，直接优化成了结果12，g内联在f中，地址是0x6

### At what address is the function printf located?

printf在地址0x628

### What value is in the register ra just after the jalr to printf in main?

ra应为函数返回后的下一条指令的地址，是0x38


### What is the output?
```c
	unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, &i);
```

由于riscv是小端存储，所以57616打印出来就是E110

对于`0x00646c72`，由于是小段储存，i的地址指向的是`0x72`，输出顺序是`0x72`->`0x00`，于是根据ASCII码表，输出应该是`rld\0`

综上，打印出的内容应该是`HE110 World`

如果使用大端储存，57616不需要变化，`0x00646c72`应变为`0x726c6400`

### In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
```c
printf("x=%d y=%d", 3);
```

会是r2寄存器的上一个数值，这是因为没有设置第3个参数，r2的值还是上一次使用r2时留下的，没有变化。