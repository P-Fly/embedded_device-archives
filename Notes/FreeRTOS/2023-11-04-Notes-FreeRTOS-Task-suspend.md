# 任务的挂起

## vTaskSuspend

### 功能

显式挂起指定任务。被挂起的任务不参与就绪任务的调度，无论优先级多高都不会获得 CPU；恢复后先进入就绪态，实际运行时间仍由调度决定。

挂起没有计数或嵌套深度。其他任务即使对同一目标连续调用多次 `vTaskSuspend()`，一次有效的 `vTaskResume()` 或 `xTaskResumeFromISR()` 就可恢复它，不需要逐次配对。这也意味着多个模块不能各自把 suspend/resume 当成独立持有的锁。

`vTaskSuspend()` 只改变指定任务的状态，其他就绪任务仍可运行；`vTaskSuspendAll()` 则挂起调度器，支持嵌套，并用 `xTaskResumeAll()` 配对恢复。恢复调度器不会恢复被显式挂起的任务，两组接口不能互相配对。

### 原型

```c
void vTaskSuspend( TaskHandle_t xTaskToSuspend );
```

### 参数

`TaskHandle_t xTaskToSuspend`

需要挂起的有效任务句柄；传入 `NULL` 表示挂起当前调用任务。任务自挂起后，须由其他执行上下文恢复，才能继续执行该调用之后的语句。普通 tick 到期不会自动恢复显式挂起的任务。

## vTaskResume

### 功能

在任务上下文恢复经 `vTaskSuspend()` 显式挂起的目标，使其进入就绪态。目标尚未挂起或已被恢复时，不会保存一次“将来可用”的恢复请求。

此接口用于显式挂起/恢复，不应作为中止普通延时、队列或通知等待的通用机制。内核在某些无限等待中也使用挂起链表作为存储位置，链表归属不等于任务被应用显式挂起。需要中止阻塞等待时，应按该等待接口的语义或 `xTaskAbortDelay()` 处理。

### 原型

```c
void vTaskResume( TaskHandle_t xTaskToResume );
```

### 参数

`TaskHandle_t xTaskToResume`

需要恢复的有效目标任务句柄，不能传入 `NULL`。正在执行的调用任务不是待恢复的挂起任务。

## xTaskResumeFromISR

### 功能

在 ISR 中恢复经 `vTaskSuspend()` 显式挂起的任务。只能从允许调用内核 `FromISR` 接口的中断优先级使用，并满足所用 port 的中断屏蔽规则。

调度器未挂起时，目标直接进入就绪态；若调度器正在挂起，内核将目标事件项加入 `xPendingReadyList`，由调度器恢复流程完成就绪迁移。这里的恢复请求同样不会累计。

### 原型

```c
BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume );
```

### 参数

`TaskHandle_t xTaskToResume`

需要恢复的有效目标任务句柄，不能传入 `NULL`。

### 返回值

`pdTRUE` 表示 ISR 应请求一次上下文切换；在调度器未挂起且成功恢复目标的路径上，目标优先级高于或等于被中断任务的优先级时返回 `pdTRUE`。其余路径返回 `pdFALSE`。这不是“恢复是否成功”的状态码：成功恢复较低优先级任务，或把恢复操作延后到调度器恢复时处理，也会返回 `pdFALSE`。

以 `portable/RVDS/ARM_CM4F` 为例，ISR 中将返回值存入 `xYieldRequired`，结束前调用 `portYIELD_FROM_ISR(xYieldRequired)`。它经 `portEND_SWITCHING_ISR()` 在标志非 `pdFALSE` 时设置 PendSV pending；接口返回和设置 pending 都不表示任务已在 ISR 内立即开始运行，实际切换还受异常优先级、中断屏蔽和调度器状态影响。其他 port 应使用各自定义的 ISR yield 形式。

这个接口直接返回切换标志，没有 `pxHigherPriorityTaskWoken` 输出参数。若 ISR 内还调用其他唤醒接口，应把它们的切换需求合并，保留已经为真的结果，不能被后续一次 `pdFALSE` 覆盖。

## 配置与调用边界

| 接口 | 所需配置与上下文 |
| --- | --- |
| `vTaskSuspend()` / `vTaskResume()` | `INCLUDE_vTaskSuspend = 1`；任务上下文。 |
| `xTaskResumeFromISR()` | 同时要求 `INCLUDE_vTaskSuspend = 1` 和 `INCLUDE_xTaskResumeFromISR = 1`；ISR 上下文。 |

自挂起可能导致任务切换，不能放在应用临界区或调度器挂起区间内。任务句柄和目标生命周期也必须由应用协调，不能用已经删除或失效的句柄恢复任务。

## 源码出处

- [tasks.c（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：`vTaskSuspend()`、`prvTaskIsTaskSuspended()`、`vTaskResume()`、`xTaskResumeFromISR()` 及待就绪链表分支。
- [include/task.h（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：挂起/恢复参数、非累计语义、配置及提前恢复的同步风险。
- [portable/RVDS/ARM_CM4F/portmacro.h（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/portmacro.h)：`portYIELD_FROM_ISR()`、`portEND_SWITCHING_ISR()` 与 PendSV 请求。
