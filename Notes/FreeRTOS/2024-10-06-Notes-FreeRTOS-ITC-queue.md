# 队列

队列用于任务之间、ISR 与任务之间传递固定大小的消息项。队尾发送和队首接收组成 FIFO，也可以将新项发送到队首。内核负责同步队列操作；多任务分别发送或接收时，各项操作按实际执行顺序完成。

![Queue Animation][1]

每次发送会复制创建时指定的 `uxItemSize` 字节，成功后发送方可以复用原消息缓冲区。接收方需提供足够大的缓冲区，接收成功后使用复制得到的数据。普通入队和出队使用已有存储，不逐项动态分配内存。

大消息可以通过指针项传递。此时队列只复制指针值，发送参数应指向保存该指针值的变量；指向的对象必须在消费者使用期间保持有效。对象的所有权、并发访问和释放由应用协调，发送成功不表示消费者已处理完成。

队列可以使用动态或静态存储。采用 MPU port 时，对内核对象和传入缓冲区的权限检查由相应 port、wrapper 与配置决定。

任务使用普通收发接口，ISR 使用该接口明确支持的 `FromISR` 版本，并满足 port 的中断优先级限制。等待时间和任务唤醒标志的含义分别见收发章节。

## 主要数据结构

### Queue_t

`Queue_t` 是内部队列控制结构，与消息数据区和等待任务链表共同组成队列。这里按 `queue.c` 中的用途解释字段；`pcHead`、`pcWriteTo` 是直接成员，`pcTail`、`pcReadFrom` 位于 `u.xQueue`，互斥量字段位于同一 union 的 `u.xSemaphore`。两组 union 成员复用空间，不能同时按两种对象解释。

`int8_t *pcHead;`

普通数据队列中指向消息存储区起点。互斥量路径将它设为 `NULL` 作为类型标记；项大小为 0 的非互斥量对象初始化时使用控制结构自身地址作为非空占位。

`int8_t *pcWriteTo;`

指向从队尾发送时的下一次写入位置。内核按 `uxItemSize` 字节拷贝一个队列项，再按项大小移动指针，到达存储区末端时回绕；队列是否有可写空间由消息计数判断。

`int8_t *pcTail;`

队列数据路径中的存储区末端后一地址，等于 `pcHead + uxLength * uxItemSize`，用于回绕判断，不是可读写槽位。其他对象使用相应的 union 分支。

`int8_t *pcReadFrom;`

记录上一次读取位置。接收时先按 `uxItemSize` 字节前移并处理回绕，再拷贝该位置的一个队列项；初始化时设为最后一个槽位，使首次接收从存储区起点读取。此字段用于队列数据路径。

`TaskHandle_t xMutexHolder;`

互斥量持有者的任务句柄，普通二值/计数信号量不以此建立任务所有权。

`UBaseType_t uxRecursiveCallCount;`

递归互斥量被当前持有者重复获取的层数，配对释放时递减。

`List_t xTasksWaitingToSend;`

按任务优先级排列的发送等待链表，挂载队列满时等待发送的任务事件项，消息数据仍存放在队列数据区。

`List_t xTasksWaitingToReceive;`

按任务优先级排列的接收等待链表，挂载队列空时等待接收或查看数据的任务事件项。

`volatile UBaseType_t uxMessagesWaiting;`

普通数据队列中当前保存的消息项数，范围为 0 至 `uxLength`。

`UBaseType_t uxLength;`

队列容量，即最多可保存的项数。

`UBaseType_t uxItemSize;`

每项复制的字节数，创建后固定。

`volatile int8_t cRxLock;`

接收事件的延后处理计数；锁定期间 ISR 移除项时累计，解锁后用于处理等待发送的任务。具体边界见队列锁章节。

`volatile int8_t cTxLock;`

发送事件的延后处理计数；锁定期间 ISR 发送项时累计，解锁后用于处理等待接收的任务或队列集通知。

`uint8_t ucStaticallyAllocated;`

同时启用 `configSUPPORT_STATIC_ALLOCATION` 和 `configSUPPORT_DYNAMIC_ALLOCATION` 时存在，用于在删除时区分应用存储和动态分配。

`struct QueueDefinition *pxQueueSetContainer;`

指向所属队列集；启用 `configUSE_QUEUE_SETS` 时存在，未加入集合时为 `NULL`。

`UBaseType_t uxQueueNumber;`

启用 `configUSE_TRACE_FACILITY` 时存在，为追踪工具保存对象编号，不参与收发调度决策。

`uint8_t ucQueueType;`

启用 `configUSE_TRACE_FACILITY` 时存在，记录追踪使用的对象类型。

## 创建队列

| API | 实际调用函数 |
| :--- | :--- |
| `xQueueCreate` | `xQueueGenericCreate` |
| `xQueueCreateStatic` | `xQueueGenericCreateStatic` |

应用通过 `xQueueCreate()` 或 `xQueueCreateStatic()` 创建普通队列，内部通用函数还服务于其他对象类型。动态创建要求 `configSUPPORT_DYNAMIC_ALLOCATION == 1`，静态创建要求 `configSUPPORT_STATIC_ALLOCATION == 1`。

动态队列一次分配控制结构和数据区，数据区大小为 `uxQueueLength * uxItemSize` 字节；实现会检查容量、乘法及加法溢出，并处理分配失败。静态队列由应用提供 `StaticQueue_t` 和足够大的数据缓冲区，两者须在队列有效期间保持有效。创建成功返回队列句柄，失败返回 `NULL`；无效参数应先由应用排除，启用断言可辅助检查。

### xQueueGenericCreate

```c
QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength,
                                   const UBaseType_t uxItemSize,
                                   const uint8_t ucQueueType )
```

- `uxQueueLength`：队列容量，必须大于 0。

- `uxItemSize`：每项字节数，普通数据队列按实际消息类型的大小设置。

- `ucQueueType`：内部对象类型，普通队列为 `queueQUEUE_TYPE_BASE`，由公开创建宏提供。

动态创建的主要调用关系如下图，图中省略参数检查和分配失败分支：

![xQueueCreate Sequence][2]

### 创建初始化与重置

`prvInitialiseNewQueue()` 调用 `xQueueGenericReset()` 初始化消息计数、环形读写位置和锁状态。新队列还会初始化两条等待链表，此时没有已阻塞的使用者。

应用调用 `xQueueReset()` 重置已有队列时，消息计数变为 0，原有消息不再可接收，但数据区内容不会被擦除。接收等待者继续等待；若有发送等待者，最多解除其中一个最高优先级任务的阻塞，并按抢占配置决定是否请求切换。被唤醒的任务仍需重新尝试发送。

队列项含指针时，重置不会释放指向的对象。应用应先安排清理或所有权转移，避免丢失最后一个引用。

下图表示非零项大小的动态队列初始化后的控制结构与数据区关系，字段按用途展开，实际 union 和条件字段布局见对应源码。静态创建时，控制结构和数据区由应用分别提供。

![xQueueCreate Structure][3]

## 入队

| API | 实际调用函数 | 入队方式 | 备注 |
| :--- | :--- | :--- | :--- |
| `xQueueSendToFront` | `xQueueGenericSend` | 从队首入队 | 任务上下文 |
| `xQueueSendToBack` | `xQueueGenericSend` | 从队尾入队 | 任务上下文 |
| `xQueueOverwrite` | `xQueueGenericSend` | 写入并替换已有值 | 仅用于长度为 1 的队列 |
| `xQueueSend` | `xQueueGenericSend` | 从队尾入队 | 保留的兼容别名，与 `xQueueSendToBack` 等价 |
| `xQueueSendToFrontFromISR` | `xQueueGenericSendFromISR` | 从队首入队 | ISR 上下文，不阻塞 |
| `xQueueSendToBackFromISR` | `xQueueGenericSendFromISR` | 从队尾入队 | ISR 上下文，不阻塞 |
| `xQueueOverwriteFromISR` | `xQueueGenericSendFromISR` | 写入并替换已有值 | 仅用于长度为 1 的队列，不阻塞 |
| `xQueueSendFromISR` | `xQueueGenericSendFromISR` | 从队尾入队 | 保留的兼容别名，与 `xQueueSendToBackFromISR` 等价 |

覆盖接口用于保存单个最新值：队列为空时新增一项，已有一项时替换旧值，消息计数保持为 1。应用需允许旧值被丢弃；项为指针时，还需安排被替换对象的所有权和回收。

### xQueueGenericSend

```c
BaseType_t xQueueGenericSend( QueueHandle_t xQueue,
                              const void * const pvItemToQueue,
                              TickType_t xTicksToWait,
                              const BaseType_t xCopyPosition )
```

- `xQueue`：有效队列句柄。
- `pvItemToQueue`：待复制项的地址，缓冲区至少包含 `uxItemSize` 字节，调用期间保持有效；若调用阻塞，返回前也应保持内容稳定。
- `xTicksToWait`：队列满时允许等待的 tick 数，0 表示立即尝试；不是毫秒数。
- `xCopyPosition`：由公开宏指定 `queueSEND_TO_BACK`、`queueSEND_TO_FRONT` 或 `queueOVERWRITE`。

发送成功返回 `pdPASS`；普通发送在零等待且队列满，或等待期限耗尽后仍不能发送时返回 `errQUEUE_FULL`。长度为 1 的合法覆盖发送不因队列已有值而等待。

### 阻塞发送与调度

以下按普通数据队列说明，不展开队列集及互斥量分支：

| 条件或阶段 | 处理 |
| --- | --- |
| 队列有空间 | 在短临界区中复制一项并更新计数；有接收等待者时解除其中一个最高优先级任务的阻塞，按抢占配置决定是否请求切换 |
| 队列满且等待为 0 | 返回 `errQUEUE_FULL` |
| 队列满且可等待 | 记录超时起点，挂起调度器并锁定队列，重新检查剩余时间及满状态 |
| 仍需阻塞 | 将当前任务事件项加入 `xTasksWaitingToSend`，状态项移出就绪链表并进入相应的等待链表，再解锁并恢复调度器 |
| 被唤醒或条件改变 | 重新检查能否发送；唤醒不为该任务预留槽位，必要时继续按剩余时间等待 |

有限等待通过 `xTaskCheckForTimeOut()` 更新剩余 tick，不在每次重试时重置完整期限。`INCLUDE_vTaskSuspend == 1` 时，这些阻塞收发接口可用 `portMAX_DELAY` 表示无限等待；否则它是有限的最大 tick 数。任务实际恢复运行还受调度和中断延迟影响。

当前任务已经阻塞时，即使没有更高优先级任务被唤醒，也需要让出 CPU；若 `xTaskResumeAll()` 未完成必要的调度，调用路径会显式请求切换。调度器选择最高优先级就绪任务，因此可能运行较低优先级任务。当前高优先级任务仍就绪时，单纯 yield 则仍按优先级规则选择任务。

应用不在挂起调度器期间调用可能引起切换的队列接口，也不在临界区中使用非零等待时间；内部收发路径按内核自身的同步协议执行。

### xQueueGenericSendFromISR

```c
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue,
                                     const void * const pvItemToQueue,
                                     BaseType_t * const pxHigherPriorityTaskWoken,
                                     const BaseType_t xCopyPosition )
```

- `xQueue`：有效队列句柄。
- `pvItemToQueue`：待复制项的地址，调用期间须可访问并包含完整项。
- `pxHigherPriorityTaskWoken`：可选输出标志指针；传入非 `NULL` 时，API 在解除更高优先级任务阻塞时将其指向的值设为 `pdTRUE`。
- `xCopyPosition`：由对应公开 ISR 宏指定入队或覆盖方式。

ISR 发送立即尝试，成功返回 `pdPASS`，普通发送遇满队列返回 `errQUEUE_FULL`。发送成功只表示数据已复制，不保证唤醒了任务。队列未锁时可处理接收等待链表；锁定期间先记录发送事件，延后到解锁处理。

### ISR 唤醒标志

在一次 ISR 的首次相关调用前，将 `xHigherPriorityTaskWoken` 初始化为 `pdFALSE`。API 只在需要时置为 `pdTRUE`，多次收发之间保留累计结果；退出 ISR 前用该结果调用 port 规定的 `portYIELD_FROM_ISR()`。

API 返回值与唤醒标志分别判断：返回值表示收发是否成功，标志表示是否需要请求更高优先级任务运行。传入 `NULL` 表示不接收该标志，调用者仍需按 port 和应用的调度要求处理切换。

## 出队

| API | 数据处理 | 调用上下文 |
| :--- | :--- | :--- |
| `xQueueReceive` | 复制队首项并移除该项 | 任务，可等待 |
| `xQueuePeek` | 复制队首项并保留该项 | 任务，可等待 |
| `xQueueReceiveFromISR` | 复制队首项并移除该项 | ISR，不阻塞 |
| `xQueuePeekFromISR` | 复制队首项并保留该项 | ISR，不阻塞 |

任务接收或查看成功返回 `pdPASS`；零等待且队列为空，或等待期限耗尽后仍为空时返回 `errQUEUE_EMPTY`。ISR 接口没有等待时间参数，成功返回 `pdPASS`，遇空队列立即返回 `pdFAIL`。失败时没有取得有效的新消息，应用应先判断返回值再使用缓冲区。

### xQueueReceive

```c
BaseType_t xQueueReceive( QueueHandle_t xQueue,
                          void * const pvBuffer,
                          TickType_t xTicksToWait )
```

- `xQueue`：有效队列句柄。
- `pvBuffer`：接收缓冲区，至少可写入 `uxItemSize` 字节；调用阻塞时也须保持有效。
- `xTicksToWait`：队列空时允许等待的 tick 数，0 表示立即尝试；超时和无限等待规则与发送接口相同。

| 条件或阶段 | 处理 |
| --- | --- |
| 队列非空 | 在短临界区中复制队首项并减少消息计数；有发送等待者时解除其中一个最高优先级任务的阻塞，按抢占配置决定是否请求切换 |
| 队列空且等待为 0 | 返回 `errQUEUE_EMPTY` |
| 队列空且可等待 | 记录超时起点，挂起调度器并锁定队列，重新检查剩余时间及空状态 |
| 仍需阻塞 | 将当前任务事件项加入 `xTasksWaitingToReceive`，状态项移出就绪链表并进入相应的等待链表，再解锁并恢复调度器 |
| 被唤醒或条件改变 | 重新检查是否有消息；唤醒不为该任务预留消息，必要时继续按剩余时间等待 |

当前任务阻塞后的切换规则与发送相同：它已不在就绪链表中，调度器可以选择较低优先级任务运行。被解除阻塞只表示可以参与调度，不等于已经运行或接收成功。

### xQueuePeek

`xQueuePeek()` 的任务参数和等待规则与 `xQueueReceive()` 相同，但成功后恢复原读取位置，不减少消息计数。后续接收者仍可取得该项；多次查看不保证消息一直不变，其他任务或 ISR 可以在两次调用之间修改队列。

任务查看成功时，内核还会检查接收等待链表，允许另一个接收等待者继续处理仍保留的消息。查看不腾出槽位，因此不因查看而唤醒发送等待者。`xQueuePeekFromISR()` 要求项大小非零，不能用于查看信号量；它只复制数据并恢复读取位置，不处理等待链表，也没有 `pxHigherPriorityTaskWoken` 参数。

### xQueueReceiveFromISR

```c
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue,
                                 void * const pvBuffer,
                                 BaseType_t * const pxHigherPriorityTaskWoken )
```

- `xQueue`：有效队列句柄。
- `pvBuffer`：调用期间可写的接收缓冲区，至少容纳一个完整项。
- `pxHigherPriorityTaskWoken`：可选输出标志指针；解除更高优先级发送等待者的阻塞时，API 将其指向的值设为 `pdTRUE`。初始化、累计及 ISR yield 规则见前文。

接收成功后减少消息计数。队列未锁时可处理发送等待链表；锁定期间先记录接收事件，延后到解锁处理。返回 `pdPASS` 不表示一定唤醒了任务，唤醒标志也不能代替接收结果。

## 删除队列

### vQueueDelete

```c
void vQueueDelete( QueueHandle_t xQueue )
```

- `xQueue`：待删除的有效队列句柄；在任务上下文调用。

删除前，应用必须确保没有任务阻塞等待该队列，并停止所有任务和 ISR 对它的访问。`vQueueDelete()` 不负责通知或解除等待者，也不替应用协调仍在进行的收发操作；可以通过退出消息、应用同步及停止相关中断源等方式先结束使用。

动态创建的队列释放其控制结构和数据区所用的内存；静态创建的队列不释放应用提供的存储。两种情况下，删除后原句柄都不再有效，内核不会自动清空应用保存的句柄副本。队列中若保存指针，指向的对象也不会被自动释放，应用需在删除前完成所有权转移或清理。

## 队列锁与同步

阻塞路径需要把“检查队列条件”和“登记任务等待”衔接起来，并允许 ISR 在此期间继续收发。内核组合使用以下机制：

| 机制 | 作用 |
| --- | --- |
| 短临界区或 ISR 中断屏蔽 | 保护消息拷贝、读写位置、计数及锁状态等共享数据；屏蔽范围由 port 决定 |
| `vTaskSuspendAll()` / `xTaskResumeAll()` | 暂停并恢复任务调度；中断仍可响应，恢复时处理延后的任务状态迁移和调度请求 |
| `prvLockQueue()` / `prvUnlockQueue()` | 延后处理队列事件等待链表，把锁定期间的 ISR 收发事件留到解锁时处理 |

未锁状态为 `queueUNLOCKED`（-1）；上锁时将未锁的 `cRxLock`、`cTxLock` 设为 `queueLOCKED_UNMODIFIED`（0）。它们分别记录锁定期间需要延后处理的接收和发送事件。

锁计数达到系统任务数后不再递增，递增前还有 `queueINT8_MAX` 上限断言检查。它们用于安排延后唤醒，不是统计全部收发次数的计数器。

队列锁不阻止 ISR 读写消息。ISR 仍在相应的中断屏蔽保护下更新数据和消息计数；队列未锁时直接处理对应等待链表，已锁时累计锁计数，暂不操作该等待链表。接收事件对应等待发送者，发送事件对应等待接收者；队列集分支还会处理集合通知。

`prvUnlockQueue()` 在调度器仍挂起时，按计数及实际等待者处理延后的事件，再把锁恢复为未锁状态。此时解除阻塞的任务通过待就绪链表暂存；真正迁移到就绪链表并决定是否切换，由后续 `xTaskResumeAll()` 完成。

因此，队列锁保护的是事件处理时序，调度器挂起保护的是任务调度过程，短临界区负责相应共享数据访问。应用不应把内部队列锁当作可调用的同步接口。

## 源码参考

- [FreeRTOS-Kernel V10.5.1 queue.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/queue.c)：控制结构、创建与重置、收发、队列锁和删除实现。
- [FreeRTOS-Kernel V10.5.1 queue.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/queue.h)：公开接口、兼容宏、返回值和使用约束。
- [FreeRTOS-Kernel V10.5.1 tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：事件等待、超时处理及恢复调度器。

 [1]: ./images/queue_animation.gif
 [2]: ./images/xQueueCreate.jpg
 [3]: ./images/xQueueCreate_Structure.jpg
