# 系统时钟

**FreeROTS** 自身的任务切换是由 **SysTick** 驱动的。在每个 **SysTick** 到来时，内核会检测是否有延时任务需要进入等待状态，同时也会检测是否需要进行任务切换。

## Setup SysTick

内核在 **vPortSetupTimerInterrupt** 中使用了一些宏来配置 **SysTick** 的频率：

 - **configCPU_CLOCK_HZ**：**CPU** 的硬件时钟频率，该频率一般是晶振直接提供或通过锁相环锁定后的值。

 - **configTICK_RATE_HZ**：系统的时钟频率，该频率代表系统1秒钟产生多少次时钟中断。

以 **STM32WB55** 的时钟树为例，**configCPU_CLOCK_HZ** 最终由 **HCLK1** 提供。

![Clock Tree][1]

## SysTick Exception

### 功能

在 **临界状态** 下执行 **xTaskIncrementTick**，并根据执行结果决定是否需要进行任务切换。

### 原型

```
void xPortSysTickHandler( void );
```

## xTaskIncrementTick

### 功能

提供系统时钟节拍服务的具体实现。

### 原型

```
BaseType_t xTaskIncrementTick( void );
```

### 返回值

如果该函数返回 **pdTRUE**，表明应该发起一次任务切换（有高优先级任务或同优先级任务需要切换）。

### 流程

![xTaskIncrementTick][2]

## 关于计数器的溢出和延时链表的交换

 1. 内核使用 **xTickCount** 来追踪 **SysTick**。假设 **xTickCount** 为 **32Bit**，那么它的取值范围是 **0 ~ 2^32^ - 1**。

 2. 当唤醒时间 **xTimeToWake** 在这个范围内时，需要被唤醒的任务属于链表 **pxDelayedTaskList**。

 3. 当唤醒时间 **xTimeToWake** 溢出时，需要被唤醒的任务属于链表 **pxOverflowDelayedTaskList**，此时需要等待 **xTickCount** 溢出后再关注该任务。

 4. 当 **xTickCount** 溢出时，不需要重新计算任务延时。只需要将两个延时链表交换即可。

![Switch Delayed Lists][3]

 [1]: ./images/clock_tree.jpg
 [2]: ./images/xTaskIncrementTick.jpg
 [3]: ./images/switch_delayed_lists.jpg
