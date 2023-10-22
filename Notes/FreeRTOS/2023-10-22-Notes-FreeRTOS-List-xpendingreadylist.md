# 特殊链表 - xPendingReadyList

**FreeRTOS** 内部有很多任务链表，几乎所有的任务链表都是使用状态项 **xStateListItem** 进行挂载。只有一个特殊的任务链表 **xPendingReadyList** 是使用消息项 **xEventListItem** 进行挂载。

因此该链表更像一个用于接收 **ISR** 消息的消息链表。

### 为什么需要使用该链表

对于 **FreeRTOS** 来说，访问临界区有三种方式：

 1. 关闭中断响应：

    - **taskENTER_CRITICAL** / **taskEXIT_CRITICAL**

    - **taskENTER_CRITICAL_FROM_ISR** / **taskEXIT_CRITICAL_FROM_ISR**

    - **taskDISABLE_INTERRUPTS** / **taskENABLE_INTERRUPTS**

 2. 使用锁机制：

    - **Semaphore**

    - **Mutex**

 3. 挂起调度器：

    - **vTaskSuspendAll** / **xTaskResumeAll**

**FreeRTOS** 为了减少丢失中断的可能，在内核中经常使用 **vTaskSuspendAll** 的方式来访问临界区。此时内核中所有任务的状态需要被锁定，不允许被修改。

这样调度器挂起期间，如果产生了中断，并且在该中断中有任务需要进入 **就绪态** 时，内核就不能将该任务直接放入等待链表中。

此时内核需要使用消息项 **xEventListItem** 将任务暂存到 **xPendingReadyList** 链表中。待调度器恢复时，内核会立即把该链表中的所有任务放置到合适的等待链表中。
