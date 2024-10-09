# 信号量和互斥量

## 信号量

### 二进制信号量

![Binary Semaphore][1]

**二进制信号量** 被认为是长度 **等于1** 的队列，只需要关心队列是 **空** 或 **满**，并不关心具体的值。

一般用于 **互斥** 和 **同步** 的目的。

**二进制信号量** 和 **互斥锁** 极为相似，但存在一些细微差别：**互斥锁** 包括 **优先级继承机制**。因此，**二进制信号量** 是实现 **同步** 的更好选择 (比如任务与任务，中断与任务之间的同步)，而 **互斥锁** 是实现 **互斥** 的更好选择 (比如对驱动资源的访问限制)。

| API | Comments |
| :--- | :--- |
| xSemaphoreCreateBinary | 创建一个二进制信号量 |
| xSemaphoreCreateBinaryStatic | 创建一个使用静态内存的二进制信号量 |
| xSemaphoreTake | 获取信号量 |
| xSemaphoreTakeFromISR | 中断中获取信号量 |
| xSemaphoreGive | 释放信号量 |
| xSemaphoreGiveFromISR | 中断中释放信号量 |
| vSemaphoreDelete | 删除信号量 |

对于 **xSemaphoreCreateBinary**，实际上就是封装了 **xQueueGenericCreate** 函数，只是不需要关心具体的值 (参数 **uxQueueLength = 1** 且 **uxItemSize = 0**)。

对于 **xSemaphoreTake**，流程几乎与 **xQueueReceive** 完全一致，只是不需要从队列中拷贝数据。

对于 **xSemaphoreTakeFromISR**，实际上就是封装了 **xQueueReceiveFromISR** 函数，只是不需要从队列中拷贝数据。

对于 **xSemaphoreGive**，实际上就是封装了 **xQueueGenericSend** 函数，只是不需要拷贝数据到队列中。

对于 **xSemaphoreGiveFromISR**，流程几乎与 **xQueueGenericSendFromISR** 完全一致，只是不需要从队列中拷贝数据。

### 计数信号量

**计数信号量** 被认为是长度 **大于1** 的队列，只需要关心队列是 **空** 或 **满**，并不关心具体的值。

通常应用于两种情况：

 - **Counting Events**：创建信号量时计数值为零。事件处理程序在每次事件发生时递增信号量计数值， 而处理程序任务将在每次处理事件时递减信号量计数值。因此，计数值是待处理的事件数。

 - **Resource Management**：创建信号量时计数值等于可用资源的数量。为了获得对资源的控制，任务必须首先递减信号量计数值。当计数值达到零时， 表示没有空闲资源可用。当任务结束使用资源时，它会递增信号量计数值。

| API | Comments |
| :--- | :--- |
| xSemaphoreCreateCounting | 创建一个计数信号量 |
| xSemaphoreCreateCountingStatic | 创建一个使用静态内存的计数信号量 |
| xSemaphoreTake | 获取信号量 |
| xSemaphoreTakeFromISR | 中断中获取信号量 |
| xSemaphoreGive | 释放信号量 |
| xSemaphoreGiveFromISR | 中断中释放信号量 |
| vSemaphoreDelete | 删除信号量 |

对于 **xSemaphoreCreateCounting**，初始化时 **uxQueueLength = N**，**uxItemSize = 0**，且会单独设置初始值 **uxMessagesWaiting**。

## 互斥锁

### 互斥锁

![mutexes][2]

**互斥锁** 是包含 **优先级继承机制的二进制信号量**。一般来说，**互斥锁** 是实现 **互斥** 的更好选择，而 **二进制信号量** 是实现 **同步** 的更好选择。

**优先级继承机制** 是指如果高优先级任务进入阻塞状态，同时尝试获取当前由低优先级任务持有的 **互斥锁**，则持有 **互斥锁** 的任务的优先级会暂时提高到阻塞任务的优先级。

**不能在中断中使用互斥锁**。

| API | Comments |
| :--- | :--- |
| xSemaphoreCreateMutex | 创建一个互斥锁 |
| xSemaphoreCreateMutexStatic | 创建一个使用静态内存的互斥锁 |
| xSemaphoreTake | 获取信号量 |
| xSemaphoreGive | 释放信号量 |
| vSemaphoreDelete | 删除信号量 |

### 递归互斥锁

**递归互斥锁** 与 **互斥锁** 相似，但用户可重复加锁同一把 **递归互斥锁**。

**不能在中断中使用递归互斥锁**。

| API | Comments |
| :--- | :--- |
| xSemaphoreCreateRecursiveMutex | 创建一个递归互斥锁 |
| xSemaphoreCreateRecursiveMutexStatic | 创建一个使用静态内存的递归互斥锁 |
| xSemaphoreTakeRecursive | 获取信号量 |
| xSemaphoreGiveRecursive | 释放信号量 |
| vSemaphoreDelete | 删除信号量 |

 [1]: ./images/binary-semaphore.gif
 [2]: ./images/mutexes.gif
