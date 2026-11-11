# 信号量和互斥量

信号量和互斥量复用内核队列结构，但项大小为 0，不复制消息数据。可用数量保存在 `uxMessagesWaiting` 中；互斥量还维护任务所有权和优先级继承相关状态。

动态创建接口要求 `configSUPPORT_DYNAMIC_ALLOCATION == 1`，静态创建接口要求 `configSUPPORT_STATIC_ALLOCATION == 1`。静态接口使用应用提供的 `StaticSemaphore_t`，该存储必须在对象有效期间保持有效。创建成功返回句柄，失败返回 `NULL`；应用应先排除无效参数，断言可辅助检查。

## 信号量

### 二值信号量

![Binary Semaphore][1]

二值信号量的计数只能是 0 或 1，对应无可用事件和有一个可用事件；内核以长度为 1、项大小为 0 的队列结构实现它。

`xSemaphoreCreateBinary()` 和 `xSemaphoreCreateBinaryStatic()` 创建后的计数都是 0，需要先 give 才能成功 take。give 在计数为 0 时将其增加到 1，计数已为 1 时失败；take 成功则将计数减为 0。未被取走前的重复 give 不会积累为多个事件。

二值信号量适合任务之间或 ISR 与任务之间的同步。它不记录持有任务，give 和 take 可以由不同执行者完成，不要求消费者为每次事件再 give 回去。它没有优先级继承；保护共享资源时，通常使用具有所有权的互斥量。

| API | 功能 |
| :--- | :--- |
| `xSemaphoreCreateBinary` | 动态创建初始为空的二值信号量 |
| `xSemaphoreCreateBinaryStatic` | 使用应用存储创建初始为空的二值信号量 |
| `xSemaphoreTake` | 任务获取信号量，可等待 |
| `xSemaphoreTakeFromISR` | ISR 获取信号量，不阻塞 |
| `xSemaphoreGive` | 任务释放信号量，不等待 |
| `xSemaphoreGiveFromISR` | ISR 释放信号量，不阻塞 |
| `vSemaphoreDelete` | 删除信号量 |

`xSemaphoreCreateBinary()` 封装 `xQueueGenericCreate()`，设置 `uxQueueLength = 1`、`uxItemSize = 0`。`xSemaphoreTake()` 调用内部的 `xQueueSemaphoreTake()`，`xSemaphoreGive()` 调用 `xQueueGenericSend()`；应用使用信号量公开接口，不直接依赖这些内部函数。

### 计数信号量

计数信号量用计数表示可用资源数或待处理事件数，而不是仅区分空和满。创建时指定 `uxMaxCount` 和 `uxInitialCount`，要求最大计数大于 0，且初始计数不大于最大计数；最大计数也可以为 1。

give 成功使计数加 1，take 成功使计数减 1。计数为 0 时不能立即获取，计数达到 `uxMaxCount` 后再次 give 会失败，不会回绕，也不会再保存一个事件。

常见用途：

- 事件计数：初始计数通常为 0，每发生一次可记录的事件就 give，处理任务每成功 take 一次就处理一个事件。容量应覆盖允许积累的事件数，应用需处理达到上限后的 give 失败。
- 资源管理：初始计数设为当前可用资源数。任务成功 take 后占用一个资源，使用结束后 give 归还；资源身份、归属和内容仍由应用管理。

| API | 功能 |
| :--- | :--- |
| `xSemaphoreCreateCounting` | 动态创建指定最大值和初始值的计数信号量 |
| `xSemaphoreCreateCountingStatic` | 使用应用存储创建计数信号量 |
| `xSemaphoreTake` | 任务获取一个计数单位，可等待 |
| `xSemaphoreTakeFromISR` | ISR 获取一个计数单位，不阻塞 |
| `xSemaphoreGive` | 任务归还资源或记录一次事件，不等待 |
| `xSemaphoreGiveFromISR` | ISR 归还资源或记录一次事件，不阻塞 |
| `vSemaphoreDelete` | 删除计数信号量 |

创建计数信号量要求 `configUSE_COUNTING_SEMAPHORES == 1`。内部设置 `uxQueueLength = uxMaxCount`、`uxItemSize = 0`，并将 `uxMessagesWaiting` 设置为 `uxInitialCount`；没有按最大计数分配消息数据区。

### 获取、释放与 ISR

任务调用 `xSemaphoreTake()`，成功返回 `pdTRUE`；无可用计数且零等待，或等待期满后仍不能获取时返回 `pdFALSE`。等待参数以 tick 为单位，0 表示立即尝试。被解除阻塞不等于获取成功，任务恢复后仍需重新检查计数。`xSemaphoreGive()` 没有等待时间参数，成功返回 `pdTRUE`，计数已达上限时返回 `pdFALSE`。

ISR 只使用支持中断上下文的 `FromISR` 接口，不阻塞，且须满足 port 的中断优先级限制。`xSemaphoreTakeFromISR()` 成功返回 `pdTRUE`，计数为 0 时返回 `pdFALSE`；`xSemaphoreGiveFromISR()` 成功返回 `pdTRUE`，计数已满时返回 `errQUEUE_FULL`。

一次 ISR 的首次相关调用前，将 `xHigherPriorityTaskWoken` 初始化为 `pdFALSE`，多次调用间保留累计结果，并在退出 ISR 前按 port 约定请求切换。返回值表示操作是否成功，唤醒标志表示是否解除更高优先级任务的阻塞，两者分别判断。`xSemaphoreGiveFromISR()` 和 `xSemaphoreTakeFromISR()` 都不能用于互斥量。

## 互斥锁

### 互斥锁

![mutexes][2]

互斥锁（互斥量）用于任务间保护共享资源。它也只有可用和已获取两种状态，但同时记录持有任务，并支持优先级继承，不等同于普通二值信号量。

创建互斥量要求 `configUSE_MUTEXES == 1`。动态和静态创建后的互斥量都初始可用，尚无持有者，第一次成功 take 后由调用任务持有。持有任务完成受保护的访问后再 give，不能由其他任务代为释放，也不能把它当作 ISR 到任务的事件通知。

普通互斥量不支持同一任务递归获取。持有者再次 take 时也会遇到不可用状态；零等待会失败，有等待时间则可能阻塞等待自己释放，不能将重复 take 当作成功加锁。

互斥量不能在 ISR 中获取或释放，没有可用于互斥量的 `FromISR` 接口。

| API | 功能 |
| :--- | :--- |
| `xSemaphoreCreateMutex` | 动态创建初始可用的互斥量 |
| `xSemaphoreCreateMutexStatic` | 使用应用存储创建初始可用的互斥量 |
| `xSemaphoreTake` | 任务获取互斥量，可等待 |
| `xSemaphoreGive` | 持有任务释放互斥量，不等待 |
| `vSemaphoreDelete` | 删除已结束使用的互斥量 |

### 优先级继承

任务尝试获取其他任务持有的互斥量并需要阻塞时，如果等待者的优先级更高，内核将持有者的当前优先级提高到等待者的优先级。零等待获取失败不会建立阻塞等待，也不会触发这次继承。继承不改变基础优先级，不会使因其他原因阻塞的持有者自动就绪。

例如，在启用抢占的情况下，L、M、H 的优先级分别为 1、2、3。L 已持有互斥量，H 运行后等待该锁；若没有继承，就绪的 M 可能抢在 L 前运行，推迟 H 获取锁。有继承时，L 的当前优先级提高到 3；只要 L 就绪，它就能先于 M 完成受保护操作并释放锁。

优先级继承用于减轻优先级反转，不是优先级上限协议，也不防止死锁。应用仍需约束持锁时间、锁获取顺序及持锁期间的阻塞操作。

普通释放路径会减少任务持有的互斥量数量；只有全部互斥量都已归还时，才把继承后的优先级恢复为基础优先级。任务同时持有多把锁时，释放其中一把不一定立即降优先级，不能把单锁示例直接推广为任意嵌套锁行为。

等待超时有单独的降优先级路径：在发生过继承且持有者只持有一把互斥量等条件满足时，按基础优先级和该锁剩余等待者的最高优先级两者中的较大值调整。多锁情况下这一实现采用简化策略，不会在每次释放或超时时完整重算所有锁的等待关系。

### 递归互斥锁

递归互斥锁要求 `configUSE_MUTEXES == 1` 和 `configUSE_RECURSIVE_MUTEXES == 1`。创建后初始可用，同样具有任务所有权和优先级继承；只有当前持有任务可以重复获取同一把锁，其他任务仍需等待其释放。

必须配对使用 `xSemaphoreTakeRecursive()` 和 `xSemaphoreGiveRecursive()`，不能混用普通 take/give 接口。每次成功的递归 take 都增加递归计数，每次由持有者成功 give 都减少计数；只有减至 0 时才真正归还互斥量并可能唤醒等待者。

例如，同一任务连续成功获取两次后，第一次释放只把递归计数从 2 减至 1，其他任务仍不能获取；第二次释放减至 0 才归还锁。获取失败不能计入成功次数，也不应为它配对释放。非持有者调用递归释放接口返回 `pdFAIL`。

递归互斥锁同样不能在 ISR 中使用；递归获取解决的是同一任务的嵌套调用，不解决不同任务之间的循环等待。

| API | 功能 |
| :--- | :--- |
| `xSemaphoreCreateRecursiveMutex` | 动态创建初始可用的递归互斥量 |
| `xSemaphoreCreateRecursiveMutexStatic` | 使用应用存储创建初始可用的递归互斥量 |
| `xSemaphoreTakeRecursive` | 获取递归互斥量，持有者可重复获取 |
| `xSemaphoreGiveRecursive` | 持有任务配对释放一次，计数归零才归还锁 |
| `vSemaphoreDelete` | 删除已结束使用的递归互斥量 |

## 等待与生命周期

任务获取接口的有限等待使用剩余 tick 计算超时，不在每次被唤醒后重新开始完整期限。`INCLUDE_vTaskSuspend == 1` 时，`portMAX_DELAY` 可以表示无限等待；否则它是有限的最大 tick 数。不要在挂起调度器期间调用可能引起切换的接口，也不要在临界区中使用非零等待时间。

删除前，应用必须停止任务和 ISR 的相关访问，确保没有任务阻塞等待对象，且互斥量已由持有者完全归还。`vSemaphoreDelete()` 复用队列删除实现，不负责唤醒等待者、解锁应用资源或协调并发。动态创建的对象释放内核分配的存储，静态对象的应用存储不被释放；删除后句柄不再有效，应用保存的句柄副本不会被自动清空。

## 源码参考

- [FreeRTOS-Kernel V10.5.1 semphr.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/semphr.h)：信号量与互斥量的公开宏、配置及返回值约定。
- [FreeRTOS-Kernel V10.5.1 queue.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/queue.c)：初始状态、计数操作、持有者和递归计数，以及等待超时的互斥量处理。
- [FreeRTOS-Kernel V10.5.1 tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：持锁计数、优先级继承、释放与等待超时后的优先级恢复。

 [1]: ./images/binary-semaphore.gif
 [2]: ./images/mutexes.gif
