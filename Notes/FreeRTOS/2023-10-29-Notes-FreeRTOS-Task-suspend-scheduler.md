# 调度器的挂起

如果任务需要长时间独占处理器，又不希望影响中断的处理，我们可以通过挂起调度器的方式来保证不会发生上下文切换。

## vTaskSuspendAll

### 功能

在不禁用中断的场景下挂起调度器，此时不会发生上下文切换。

需要注意也不能在该状态下调用可能导致上下文切换的函数，比如 **vTaskDelay**，**vTaskDelayUntil**，**xQueueSend** 等。

### 原型

```
void vTaskSuspendAll( void );
```

## xTaskResumeAll

### 功能

恢复调度器。

需要注意该函数不会恢复由 **vTaskSuspend** 挂起的任务。

### 原型

```
BaseType_t xTaskResumeAll( void );
```

### 返回值

如果该函数返回真，表明已经发生过一次上下文切换（有高优先级任务或同优先级任务需要切换）。

### 流程

![xTaskResumeAll][1]

 [1]: ./images/xTaskResumeAll.jpg
