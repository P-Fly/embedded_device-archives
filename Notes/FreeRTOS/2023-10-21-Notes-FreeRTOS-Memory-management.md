# 内存管理

标准 **malloc** 和 **free** 的内存分配方案有以下缺点：

 - they are not always available on embedded systems.

 - they take up valuable code space.

 - they are not thread safe.

 - they are not deterministic (the amount of time taken to execute the function will differ from call to call).

因此 **FreeRTOS** 将内存分配的接口放置在了可移植层中，而除了5种默认的内存管理方案，用户也可以自由定制针对特定硬件的内存方案。

## 内存管理方案

### heap_1.c

最简单的内存管理方案，但不允许在分配了内存后释放内存。

### heap_2.c

允许释放已经分配的块，但它不会将相邻的空闲内存块组合成一个更大的空闲内存块。建议用 **heap_4.c** 来替代。

### heap_3.c

简单的封装了 **malloc** 和 **free**，并实现了线程安全性。

### heap_4.c

允许释放已经分配的块，也会将相邻的空闲内存块组合成一个更大的空闲内存块。对于大部分应用都推荐使用该方案。

### heap_5.c

使用 **heap_4.c** 中的合并算法，并且允许程序设置多个非连续的内存堆（这样可以同时管理内部快速访问的内存堆和外部低速访问的内存堆）。
