# 空闲任务

## prvIdleTask

### 功能

当调度器启动时，内核将自动创建空闲任务。它是以最低优先级创建的，以确保始终有一个任务能够运行。主要功能如下：

 - 释放需要延时释放的任务资源。

 - 提供低功耗支持。

### 原型

```
void prvIdleTask( void *pvParameters );
```

### 流程

![prvIdleTask][1]

## 低功耗tickless模式

内核会使用 **portSUPPRESS_TICKS_AND_SLEEP** 实现低功耗模式，实现方式在不同的硬件上有所不同，但大致的流程如下：

 - 估算低功耗可以维持的周期数。

 - 配置 **外部低功耗时钟** 或重新配置 **SysTick**。

 - 使用 **WFI指令** 进入低功耗状态，等待系统被唤醒。

 - 系统被唤醒，首先处理唤醒中断。

 - 恢复 **SysTick** 的配置，并设置 **xPendedTicks**。

 [1]: ./images/prvIdleTask.jpg
