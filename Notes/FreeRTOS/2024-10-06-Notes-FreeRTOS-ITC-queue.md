# 队列

**Queue** 是任务间通信的主要形式。可以用于在 **任务之间** 以及 **中断和任务之间** 传输消息。在大多数情况下，队列用作 **线程安全的FIFO缓冲区**，新数据被发送到队列的后面，但也可以发送到前面。

![Queue Animation][1]

主要特点如下：

 1. 数据交互使用 **复制** 而不是引用的方式。

    - 对于短消息，不需要内存动态申请，可以减少内存碎片。

    - 对于大数据量的消息，可以通过传递指针的方式实现数据交互。

 2. 内核负责分配队列存储区域的内存，并提供数据的发送和读取接口，因此容易通过 **MPU** 实现权限管理。

 3. 中断中不能使用不以 **FromISR** 结尾的函数。

## 主要数据结构

### Queue_t

**Queue_t** 是队列通信的基础，其成员如下：

> int8_t *pcHead;

 - 当该结构用于 **Queue** 时，该成员用作标记，指向队列存储区域的开始处。

 - 当该结构用于 **Mutex** 时，该成员为 **NULL**。

 - 用于其它类型时，该成员指向结构体的首地址。

> int8_t *pcWriteTo;

指向队列存储区域中，下一个待写入的空闲位置。(即新数据使用 **\*(pcWriteTo++) = Item** 的方式写入)。

> int8_t *pcTail;

 - 当该结构用于 **Queue** 时，该成员用作标记，指向队列存储区域的结束处。

 - 用于其它类型时，该成员为 **NULL**。

> int8_t *pcReadFrom;

指向队列存储区域中，最后一次读取队列项所在的位置。(即使用 **Item = \*(++pcReadFrom)** 的方式读取)。队列类型为 **Queue** 时使用该结构。

> TaskHandle_t xMutexHolder;

记录保持互斥的任务的句柄。队列类型为 **Semaphore**时使用该结构。

> UBaseType_t uxRecursiveCallCount;

记录被递归的次数。队列类型为 **Semaphore** 时使用该结构。

> List_t xTasksWaitingToSend;

事件链表的链表头。按照任务的优先级，挂载因等待向该队列发送消息而处于阻塞状态的任务。

> List_t xTasksWaitingToReceive;

事件链表的链表头。按照任务的优先级，挂载因等待从该队列获取消息而处于阻塞状态的任务。

> volatile UBaseType_t uxMessagesWaiting;

当前队列上的消息个数。

> UBaseType_t uxLength;

定义 **Item** 的个数。

> UBaseType_t uxItemSize;

定义每个 **Item** 的大小。

> volatile int8_t cRxLock;

队列的 **读锁**。

> volatile int8_t cTxLock;

队列的 **写锁**。

> uint8_t ucStaticallyAllocated;

标识队列是 **静态内存** 还是 **动态内存**。

> struct QueueDefinition *pxQueueSetContainer;

指向队列所属的消息组。

> UBaseType_t uxQueueNumber;

调试追踪时使用，对该队列提供一个可以追溯的数值，**FreeRTOS** 不使用该值。

> uint8_t ucQueueType;

调试追踪时使用，记录当前队列的类型。

## 创建队列

| API | 实际调用函数 |
| :--- | :--- |
| xQueueCreate | xQueueGenericCreate |
| xQueueCreateStatic | xQueueGenericCreateStatic |

**xQueueGenericCreate** 与 **xQueueGenericCreateStatic** 的区别在于所需的内存是否为动态分配，因此我们以 **xQueueGenericCreate** 为例进行分析。

### xQueueGenericCreate

```
QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength,
                                   const UBaseType_t uxItemSize,
                                   const uint8_t ucQueueType )
```

 - **uxQueueLength**：队列可以包含的最大项数。

 - **uxItemSize**：队列中每一项所需的字节数。

 - **ucQueueType**：队列类型，对于队列该参数为 **queueQUEUE_TYPE_BASE**。

使用 **xQueueGenericCreate** 创建队列的流程如下图：

![xQueueCreate Sequence][2]

需要注意 **xQueueGenericReset** 中，对阻塞任务的处理策略：

 - 如果有任务处于等待读取的状态，则该阻塞任务将继续阻塞。

 - 如果有任务处于等待写入的状态，则取消 **优先级最高的任务** 的阻塞状态。

 > If there are tasks blocked waiting to read from the queue, then the tasks will remain blocked as after this function exits the queue will still be empty.  If there are tasks blocked waiting to write to the queue, then one should be unblocked as after this function exits it will be possible to write to it.

队列创建后的数据结构如下图：

![xQueueCreate Structure][3]

## 入队

| API | 实际调用函数 | 入队方式 | 备注 |
| :--- | :--- | :--- | :--- |
| xQueueSendToFront        | xQueueGenericSend | 从队列首部入队 |  |
| xQueueSendToBack         | xQueueGenericSend | 从队列尾部入队 |  |
| xQueueOverwrite          | xQueueGenericSend | 从队列尾部覆盖式入队 | 只用于长度为1的队列 |
| xQueueSend               | xQueueGenericSend | 从队列尾部入队 | 不推荐使用，该接口存在是为了保持版本的向后兼容性 |
| xQueueSendToFrontFromISR | xQueueGenericSendFromISR | 从队列首部入队 |  |
| xQueueSendToBackFromISR  | xQueueGenericSendFromISR | 从队列尾部入队 |  |
| xQueueOverwriteFromISR   | xQueueGenericSendFromISR | 从队列尾部覆盖式入队 | 只用于长度为1的队列 |
| xQueueSendFromISR        | xQueueGenericSendFromISR | 从队列尾部入队 | 不推荐使用，该接口存在是为了保持版本的向后兼容性 |

### xQueueGenericSend

```
BaseType_t xQueueGenericSend( QueueHandle_t xQueue,
                              const void * const pvItemToQueue,
                              TickType_t xTicksToWait,
                              const BaseType_t xCopyPosition )
```

 - **xQueue**：队列的句柄。

 - **pvItemToQueue**：需要入队的 **Item**。

 - **xTicksToWait**：入队任务被阻塞的最大时间，如果设置为 **0** 则不被阻塞。

 - **xCopyPosition**：设置入队方式，可以为 **queueSEND_TO_BACK**，**queueSEND_TO_FRONT**，**queueOVERWRITE**。

使用 **xQueueGenericSend** 入队的流程如下图：

![xQueueGenericSend Sequence][4]

 - 图中绿色部分为队列未满，能正常写入的流程。

 - 红色部分为队列的阻塞和唤醒流程：

    - 当有高优先级任务被唤醒时，**xTaskResumeAll** 产生上下文切换。

    - 当没有高优先级任务被唤醒时，**portYIELD_WITHIN_API** 产生上下文切换，此时切换至同优先级或低优先级任务。

### xQueueGenericSendFromISR

```
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue,
                                     const void * const pvItemToQueue,
                                     BaseType_t * const pxHigherPriorityTaskWoken,
                                     const BaseType_t xCopyPosition )
```

 - **xQueue**：队列的句柄。

 - **pvItemToQueue**：需要入队的 **Item**。

 - **pxHigherPriorityTaskWoken**： 当该值返回 **True** 时，表明有更高优先级的任务被解除阻塞，需要在退出中断前发起一次上下文切换。

 - **xCopyPosition**：设置入队方式，可以为 **queueSEND_TO_BACK**，**queueSEND_TO_FRONT**，**queueOVERWRITE**。

使用 **xQueueGenericSendFromISR** 入队的流程如下图：

![xQueueGenericSendFromISR Sequence][5]

## 出队

| API | 出队方式 |
| :--- | :--- |
| xQueueReceive | 出队并删除数据项 |
| xQueuePeek | 出队不删除数据项 |
| xQueueReceiveFromISR | 出队并删除数据项 |
| xQueuePeekFromISR | 出队不删除数据项 |

**xQueueReceive** 与 **xQueuePeek** 的区别在于是否删除队列中出队的数据，因此我们以 **xQueueReceive** 和 **xQueueReceiveFromISR** 为例进行分析。

### xQueueReceive

```
BaseType_t xQueueReceive( QueueHandle_t xQueue,
                          void * const pvBuffer,
                          TickType_t xTicksToWait )
```

 - **xQueue**：队列的句柄。

 - **pvBuffer**：接受出队 **Item** 的 **Buffer**。

 - **xTicksToWait**：出队任务被阻塞的最大时间，如果设置为 **0** 则不被阻塞。

使用 **xQueueReceive** 出队的流程如下图：

![xQueueReceive Sequence][6]

 - 图中绿色部分为队列未空，能正常读取的流程。

 - 红色部分为队列的阻塞和唤醒流程：

    - 当有高优先级任务被唤醒时，**xTaskResumeAll** 产生上下文切换。

    - 当没有高优先级任务被唤醒时，**portYIELD_WITHIN_API** 产生上下文切换，此时切换至同优先级或低优先级任务。

### xQueueReceiveFromISR

```
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue,
                                 void * const pvBuffer,
                                 BaseType_t * const pxHigherPriorityTaskWoken )
```

 - **xQueue**：队列的句柄。

 - **pvBuffer**：接受出队 **Item** 的 **Buffer**。

 - **pxHigherPriorityTaskWoken**：当该值返回 **True** 时，表明有更高优先级的任务被解除阻塞，需要在退出中断前发起一次上下文切换。

使用 **xQueueReceiveFromISR** 出队的流程如下图：

![xQueueReceiveFromISR Sequence][7]

## 删除队列

### vQueueDelete

```
void vQueueDelete( QueueHandle_t xQueue )
```

 - **xQueue**：队列的句柄。

## 其它

### 关于队列锁

**FreeRTOS** 为了提高中断的响应时间，在队列 **阻塞** 和 **唤醒** 操作时并不会关闭中断，而是使用 **vTaskSuspendAll** 和 **xTaskResumeAll** 以关闭调度器的方式进行临界区保护。当调度器关闭时，**Task** 虽然不会被切换，但是 **ISR** 依然会被响应。因此如果 **ISR** 操作队列的事件列表 **xTasksWaitingToSend** 或 **xTasksWaitingToReceive**，则很有可能会与 **Task** 产生竞争冲突。对此系统通过使用 **prvLockQueue** 和 **prvUnlockQueue** 对队列进行锁操作，来避免这种竞争冲突。

**队列锁** 的简要逻辑是：

 - 对于 **ISR**，如果队列未上锁，则不但操作队列数据，同时还唤醒阻塞队列。

 - 对于 **ISR**，如果队列上锁，则只操作队列数据，并通过该锁记录数据的操作次数。

 - 对于 **Task**，在队列解锁时会检查锁的状态，如果有中断对队列进行了锁操作，则在此时才唤醒阻塞队列。

 [1]: ./images/queue_animation.gif
 [2]: ./images/xQueueCreate.jpg
 [3]: ./images/xQueueCreate_Structure.jpg
 [4]: ./images/xQueueGenericSend.jpg
 [5]: ./images/xQueueGenericSendFromISR.jpg
 [6]: ./images/xQueueReceive.jpg
 [7]: ./images/xQueueReceiveFromISR.jpg
