# 特殊链表 - xPendingReadyList

`xPendingReadyList` 用于暂存需要在恢复调度器时加入就绪链表的任务。

## 两种任务链表项

TCB 中的两个链表项承担不同职责：

| 链表项 | 用途 |
| --- | --- |
| `xStateListItem` | 将任务挂入就绪、延时、挂起等状态管理链表 |
| `xEventListItem` | 将任务挂入事件等待链表，或在延后就绪处理时挂入 `xPendingReadyList` |

例如，任务等待队列数据且指定有限超时时间时，状态项位于延时链表，事件项位于队列的接收等待链表。同一任务可以通过两个不同的节点同时关联这两类链表。

这种分工使内核可以用事件项暂存待就绪任务，同时保留状态项原来的链表归属。

## 调度器挂起与同步机制

下面列出几种常见同步机制及其保护范围：

| 机制 | 作用与限制 |
| --- | --- |
| `taskENTER_CRITICAL` / `taskEXIT_CRITICAL` | 用于短临界区，中断屏蔽范围由 port 决定；支持优先级屏蔽的 Cortex-M port 不一定屏蔽所有中断 |
| `taskDISABLE_INTERRUPTS` / `taskENABLE_INTERRUPTS` | 直接操作 port 的中断屏蔽机制，不应与临界区的嵌套和恢复接口混用 |
| 信号量、互斥量 | 用于事件同步或按约定保护共享资源，不会在整个资源使用期间屏蔽 ISR；互斥量有任务所有权，不能在 ISR 中使用 |
| `vTaskSuspendAll` / `xTaskResumeAll` | 暂停任务切换并推迟部分调度处理，ISR 仍可执行 |

ISR 中需要使用端口支持的中断屏蔽保存/恢复接口，例如 `taskENTER_CRITICAL_FROM_ISR` / `taskEXIT_CRITICAL_FROM_ISR`，并满足该 port 对中断优先级的限制。任务上下文和 ISR 上下文应分别使用对应接口。

内核在部分操作中挂起调度器，保留 ISR 响应，并将部分调度处理延后到恢复阶段。与 ISR 共享的数据仍需通过适当的中断屏蔽或专门的同步设计保护；恢复调度器时会处理积累的调度工作。

调度器挂起期间，应用不得调用可能导致阻塞或上下文切换的 API，例如 `xQueueSend()`。嵌套挂起需要成对恢复；由 `vTaskSuspend()` 显式挂起的任务，需要通过任务恢复接口单独恢复。

## 为什么需要暂存待就绪任务

以任务等待队列事件、事件处理进入 `xTaskRemoveFromEventList()` 为例：

1. 从事件等待链表取出待解除阻塞的任务，移除其 `xEventListItem`。
2. 如果调度器未挂起，移除该任务的 `xStateListItem`，再将状态项加入对应优先级的就绪链表。
3. 如果调度器已挂起，则把事件项加入 `xPendingReadyList`，暂不移动状态项，等待恢复调度器时完成后续迁移。

第三种情况下，事件等待关系已经改变，状态项暂留原链表，等待恢复调度器时完成迁移。有限超时任务的状态项通常留在延时链表；符合无限期等待条件的任务，其状态项也可能位于 `xSuspendedTaskList`，这种用法表示无限期阻塞。

调用 `xTaskRemoveFromEventList()` 时需要处于相应临界区。若队列处于锁定状态，相关事件先记录在队列锁计数中，待队列解锁时再处理任务唤醒。

`xTaskResumeFromISR()` 在调度器挂起时也会使用该链表暂存待恢复任务。其他事件唤醒路径是否使用该链表，取决于对应内核接口的实现。

## 恢复调度器时的处理顺序

`xTaskResumeAll()` 在临界区内递减调度器挂起计数。只有最外层挂起被解除，即计数减到零时，才完成以下处理：

1. 遍历 `xPendingReadyList`，移除各任务的事件项，再从原状态链表移除状态项，并把状态项加入对应优先级的就绪链表。
2. 必要时更新下一个任务超时的时间记录，避免继续使用迁移前的记录。
3. 补处理挂起期间积累在 `xPendedTicks` 中的 tick，使因超时需要解除阻塞的任务得到处理。
4. 根据待切换标志和抢占配置决定是否请求任务切换。

先处理已经发生的事件唤醒，再补处理积累的 tick，避免这些任务仍留在原延时链表中参与后续超时处理。挂起期间，相关 tick 处理会累计 `xPendedTicks`；`xTickCount` 的推进与到期任务处理延后到恢复阶段。

任务加入就绪链表后，便可参与调度。具体哪个任务获得 CPU，由任务优先级、抢占配置和端口的切换机制决定。

## 源码依据

- [tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：`xTaskRemoveFromEventList()`、`xTaskResumeFromISR()`、`xTaskResumeAll()`、`xTaskIncrementTick()`。
- [task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：`vTaskSuspendAll()` 的调用限制和 `xTaskResumeAll()` 的接口说明。
