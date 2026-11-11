# 任务通知

每个任务都有自己的 **通知值** 和 **通知状态**。在支持通知数组的版本中，它们分别保存在 `ulNotifiedValue[]` 和 `ucNotifyState[]` 中，每个索引都有独立的值和状态。

通知状态包括 **未等待通知**、**正在等待通知** 和 **通知待处理**，与任务的 **运行、就绪、阻塞、挂起** 状态不是同一概念。

**任务通知** 是直接发送到任务的事件，而不会通过中间对象间接发送至任务。

成功发送任务通知后，对应索引的通知会变为 **待处理**。如果目标任务正阻塞等待该索引的通知，则会被解除阻塞，进入 **就绪态**；何时实际运行由调度器决定。发送通知不会自动恢复被 `vTaskSuspend` 显式挂起的任务，也不会解除与该通知无关的阻塞。

- 该功能从 **v8.2.0** 开始可用。

- 从 **v10.4.0** 起支持通知数组，数组长度由 `configTASK_NOTIFICATION_ARRAY_ENTRIES` 配置，默认值为 1。

## 发送动作

`xTaskNotify` 系列接口通过 `eAction` 指定通知值的更新方式：

| 动作 | 对通知值的操作 | 返回值 |
| --- | --- | --- |
| `eNoAction` | 保持原值，仅发送通知 | `pdPASS` |
| `eSetBits` | 将原值与 `ulValue` 按位或 | `pdPASS` |
| `eIncrement` | 原值加 1，忽略 `ulValue` | `pdPASS` |
| `eSetValueWithOverwrite` | 无条件赋值为 `ulValue` | `pdPASS` |
| `eSetValueWithoutOverwrite` | 无待处理通知时赋值；已有待处理通知时保留原值 | 赋值成功为 `pdPASS`，已有待处理通知为 `pdFAIL` |

发送成功后，对应索引的状态为通知待处理，包括保持通知值不变的 `eNoAction`。无覆盖发送失败时，原有通知值和待处理状态保留。返回值表示发送动作是否成功。

## 接收方式与索引

| 接收接口 | 等待条件 | 返回值与处理方式 |
| --- | --- | --- |
| `xTaskNotifyWait` / `xTaskNotifyWaitIndexed` | 对应索引有待处理通知 | 收到通知返回 `pdTRUE`，未收到返回 `pdFALSE`；可读出通知值并按参数清除指定的位 |
| `ulTaskNotifyTake` / `ulTaskNotifyTakeIndexed` | 对应索引的通知值非零 | 返回清零或递减前的值；`xClearCountOnExit` 为 `pdTRUE` 时清零，为 `pdFALSE` 时减 1 |

`xTaskNotifyWait` 仅在没有待处理通知时清除 `ulBitsToClearOnEntry` 指定的位；收到通知后，先输出通知值，再清除 `ulBitsToClearOnExit` 指定的位，并消费待处理状态。

`eNoAction` 适合配合 `xTaskNotifyWait` 进行纯事件唤醒。计数用途使用 Give/Take 接口配对；Take 在等待结束后计数仍为零时返回 0，应用应按实际计数处理事件。

非 Indexed 接口使用索引 0，Indexed 接口指定目标索引。发送方和接收方必须使用同一索引；一个任务一次只能阻塞等待一个索引，各索引的通知值和状态独立保存。

流缓冲区和消息缓冲区的默认通知完成路径及阻塞等待使用索引 0。需要同时处理独立业务通知时，可将数组长度设为至少 2，并将业务收发接口统一配置到索引 1 等其他索引。例如，ISR 向索引 1 发送计数通知时，任务也应从索引 1 获取计数。

## 使用限制

- 每个任务接收自己的通知，可以有多个发送方；需要多个接收任务竞争同一资源时，应选用对应的共享同步对象。
- 接收任务可以阻塞等待通知，发送接口立即返回。无覆盖发送遇到已有待处理通知时，由发送方根据失败返回值决定后续处理。
- 任务通知没有互斥量的持有者管理和优先级继承机制。保护具有任务所有权的共享资源时应使用互斥量。

## 样例

### 替代二进制信号量

适用于单个任务等待事件的场景。Give 将通知值加 1，Take 使用 `pdTRUE` 一次清空已有计数；多次 Give 可以合并为一次唤醒，Take 返回的清零前计数可能大于 1。

| API | 功能 | 备注 |
| :--- | :--- | :--- |
| `ulTaskNotifyTake` | 等待并获取计数 | `xClearCountOnExit = pdTRUE` |
| `ulTaskNotifyTakeIndexed` | 等待并获取指定索引计数 | `xClearCountOnExit = pdTRUE` |
| `xTaskNotifyGive` | 发送计数通知 | 索引 0 |
| `xTaskNotifyGiveIndexed` | 发送指定索引计数通知 |  |
| `vTaskNotifyGiveIndexedFromISR` | 从 ISR 发送指定索引计数通知 |  |
| `vTaskNotifyGiveFromISR` | 从 ISR 发送计数通知 | 索引 0 |

### 替代计数信号量

适用于向单个任务累计事件次数。Take 使用 `pdFALSE` 每次消耗一个计数；通知值是 32 位无符号数，递增没有饱和检查，达到最大值后再次递增会回绕为零。

| API | 功能 | 备注 |
| :--- | :--- | :--- |
| `ulTaskNotifyTake` | 等待并获取计数 | `xClearCountOnExit = pdFALSE` |
| `ulTaskNotifyTakeIndexed` | 等待并获取指定索引计数 | `xClearCountOnExit = pdFALSE` |
| `xTaskNotifyGive` | 发送计数通知 | 索引 0 |
| `xTaskNotifyGiveIndexed` | 发送指定索引计数通知 |  |
| `vTaskNotifyGiveIndexedFromISR` | 从 ISR 发送指定索引计数通知 |  |
| `vTaskNotifyGiveFromISR` | 从 ISR 发送计数通知 | 索引 0 |

### 替代事件组

使用 `eSetBits` 合并事件位，接收任务通过 `xTaskNotifyWait` 读取并按需清除。该索引收到通知即可结束等待，应用自行检查所需位的组合条件；同一位被多次设置只保留置位结果。需要多个任务等待同一组事件，或由内核判断任一位/全部位条件时，使用事件组。

| API | 功能 | 备注 |
| :--- | :--- | :--- |
| `xTaskNotifyWait` | 等待并读取通知值 | 索引 0 |
| `xTaskNotifyWaitIndexed` | 等待并读取指定索引通知值 |  |
| `xTaskNotify` | 发送位通知 | `eSetBits` |
| `xTaskNotifyIndexed` | 发送指定索引位通知 | `eSetBits` |
| `xTaskNotifyFromISR` | 从 ISR 发送位通知 | `eSetBits` |
| `xTaskNotifyIndexedFromISR` | 从 ISR 发送指定索引位通知 | `eSetBits` |

### 替代邮箱

适用于向单个任务传递一个 32 位值。`eSetValueWithOverwrite` 保留最后一次写入的值；`eSetValueWithoutOverwrite` 在前一通知仍待处理时返回 `pdFAIL`。需要按顺序保存多条消息时使用队列。

| API | 功能 | 备注 |
| :--- | :--- | :--- |
| `xTaskNotifyWait` | 等待并读取通知值 | 索引 0 |
| `xTaskNotifyWaitIndexed` | 等待并读取指定索引通知值 |  |
| `xTaskNotify` | 发送通知值 | 按是否允许覆盖选择动作 |
| `xTaskNotifyIndexed` | 发送指定索引通知值 | 按是否允许覆盖选择动作 |
| `xTaskNotifyFromISR` | 从 ISR 发送通知值 | 按是否允许覆盖选择动作 |
| `xTaskNotifyIndexedFromISR` | 从 ISR 发送指定索引通知值 | 按是否允许覆盖选择动作 |

## ISR 中发送通知

ISR 使用对应的 FromISR 接口，并满足 port 对可调用内核 API 的中断优先级限制。进入 ISR 时将 `xHigherPriorityTaskWoken` 初始化为 `pdFALSE`，把它的地址传给通知接口；同一 ISR 内多次调用时保留已置位的结果。

如果该标志为 `pdTRUE`，则按 port 的约定请求在 ISR 退出时切换任务。`xTaskNotifyFromISR` 系列的返回值表示发送动作是否成功，是否需要切换由这个输出标志指示；`vTaskNotifyGiveFromISR` 系列没有返回值。

## 源码依据

- [tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：`xTaskGenericNotify()`、`xTaskGenericNotifyFromISR()`、`xTaskGenericNotifyWait()`、`ulTaskGenericNotifyTake()`。
- [task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：通知动作、Indexed 与非 Indexed 接口及 FromISR 调度约定。
- [stream_buffer.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/stream_buffer.c)：流缓冲区和消息缓冲区的默认通知路径。
