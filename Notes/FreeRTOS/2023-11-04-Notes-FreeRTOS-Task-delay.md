# 任务的延时

## vTaskDelay

### 功能

按调用时的 tick 值进行相对延时。`xTicksToDelay > 0` 时，任务进入阻塞态，到期后进入就绪态；何时实际重新运行取决于调度和更高优先级任务的执行情况。

延时受 tick 相位影响：调用可能位于两个 tick 之间，所以 `vTaskDelay(1)` 到期前的实际时间可能不足一个 tick 周期。正数 N 个 tick 的等待，以正常连续 tick 计时，到期前约为 N-1 到 N 个 tick 周期，再叠加实际获得 CPU 的调度延迟。

`vTaskDelay(0)` 不进入延时阻塞，而是请求一次 yield；如果调用任务仍是应运行的就绪任务，也可能继续由它执行，不保证低优先级任务获得 CPU。

### 原型

```c
void vTaskDelay( const TickType_t xTicksToDelay );
```

### 参数

`const TickType_t xTicksToDelay`

需要延时的 tick 数。可用 `pdMS_TO_TICKS()` 将毫秒值转换为 tick；默认整数换算会截去不足一个 tick 的部分，小间隔可能变成 `0`，使用前应检查换算结果。

## xTaskDelayUntil

### 功能

按上一周期的计划唤醒 tick 加上固定周期，计算下一次计划唤醒 tick。若目标仍在未来，任务阻塞至目标 tick 后进入就绪态；若目标已到或已过，则不为这一目标阻塞。

与每次从当前 tick 起算的 `vTaskDelay()` 相比，这种方式不把每轮执行时间自动叠加到周期中，可避免相对延时造成的持续漂移。它维持的是计划时间基准，不保证每轮都准时获得 CPU。高优先级任务抢占、调度延迟和单次工作执行超期仍可能造成 missed deadline。

### 原型

```c
BaseType_t xTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement );
```

### 参数

`TickType_t * const pxPreviousWakeTime`

指向调用方持有的计划 tick 变量。进入周期循环前，先以 `xTaskGetTickCount()` 的结果初始化该变量一次，再在各轮调用中传入它的地址。每次调用都会将它推进 `xTimeIncrement`，包括目标已经过去、任务没有进入阻塞的调用；它不记录实际得到 CPU 的时间。不要每轮重新赋为当前 tick，否则会改变计划时间基准。

`const TickType_t xTimeIncrement`

周期的 tick 数，必须大于 `0`。使用 `pdMS_TO_TICKS()` 换算周期时，也须确保结果非零。

### 返回值

返回 `pdTRUE` 表示此次调用进入了延时阻塞，返回 `pdFALSE` 表示目标已到或已过、没有为它阻塞。返回 `pdTRUE` 也不证明任务按时执行，阻塞还可能被 `xTaskAbortDelay()` 提前终止。

## xTaskAbortDelay

### 功能

由任务上下文强制另一个阻塞任务退出等待并进入就绪态，可用于中止时间等待或队列、通知等事件等待。它不提供 ISR 版本，也不用于恢复显式挂起的任务。目标就绪后是否立即运行仍由调度决定。

### 原型

```c
BaseType_t xTaskAbortDelay( TaskHandle_t xTask );
```

### 参数

`TaskHandle_t xTask`

需要操作的有效任务句柄，不能用 `NULL` 代替目标任务。

### 返回值

目标处于阻塞态且被解除阻塞时返回 `pdPASS`，其值与 `pdTRUE` 相同；目标不在阻塞态时返回 `pdFAIL`，其值与 `pdFALSE` 相同。返回成功仅表示中止等待成功，不表示目标已经获得队列数据、通知或其他资源，目标仍须检查原等待接口的结果。

若中止的是周期延时，`pxPreviousWakeTime` 已在延时调用中推进，中止等待不会自动重置这个计划 tick。

## 配置与调用边界

| 接口 | 所需配置 |
| --- | --- |
| `vTaskDelay()` | `INCLUDE_vTaskDelay = 1`。 |
| `xTaskDelayUntil()` | `INCLUDE_xTaskDelayUntil = 1`。 |
| `xTaskAbortDelay()` | `INCLUDE_xTaskAbortDelay = 1`。 |

这些是任务接口，不从 ISR 调用。会进入阻塞的延时调用不能放在应用临界区或调度器挂起区间内。

## 源码出处

- [tasks.c（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：`vTaskDelay()`、`xTaskDelayUntil()`、`xTaskAbortDelay()` 及 `prvAddCurrentTaskToDelayedList()`。
- [include/task.h（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：延时接口、返回值和配置要求。
- [include/projdefs.h（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/projdefs.h)：默认 `pdMS_TO_TICKS()` 整数换算及 `pdPASS` / `pdFAIL` 与 `pdTRUE` / `pdFALSE` 的对应关系。
