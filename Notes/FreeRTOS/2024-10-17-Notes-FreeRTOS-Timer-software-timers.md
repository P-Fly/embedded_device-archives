# 软件定时器

软件定时器能够在未来设定的时间到来时，执行用户的定时器回调函数。

定时器回调函数：在定时器服务任务的上下文中执行，不是 ISR，也不保证在到期时刻立即运行。所有回调和定时器命令由同一个服务任务串行处理，长回调会拖延其他回调及命令处理。

回调不能阻塞：不调用延时 API；调用可能等待队列、信号量或定时器命令队列的 API 时，等待参数必须为零，并检查返回值。命令队列满时应处理 `pdFAIL`，不能在回调里阻塞等待服务任务自己腾出空间，也不要忙循环重试。耗时工作宜通过非阻塞通知或队列移交给工作任务，并明确通知或入队失败时的应用策略。

FreeRTOS 通过 Timer Service Daemon 管理软件定时器。该任务是一个单独的线程，应用通过命令队列 `xTimerQueue` 与其通信。

## Timer Service Daemon

应用通过命令队列与定时器服务任务通信的示意：

 ![Timer service daemon][1]

### 配置与容量

| 配置项 | 含义与约束 |
| --- | --- |
| `configUSE_TIMERS` | 设为 1，启用软件定时器和服务任务 |
| `configTIMER_TASK_PRIORITY` | 服务任务的优先级，影响命令与回调何时获得 CPU |
| `configTIMER_TASK_STACK_DEPTH` | 动态创建服务任务时的栈深度，单位为 `StackType_t` 项。静态创建时，实际栈缓冲区及深度由 `vApplicationGetTimerTaskMemory` 提供 |
| `configTIMER_QUEUE_LENGTH` | 命令队列能容纳的消息数 |

### 周期与命令等待

`xTimerPeriodInTicks` 是定时器周期，单位为 tick，必须大于零；用 `pdMS_TO_TICKS` 转换短时间时仍需检查结果非零。

命令 API 的 `xTicksToWait` 是命令队列已满时，调用任务最多等待多少 tick 以便入队。设为零表示不等队列空间；返回 `pdFAIL` 时，本次命令未入队，应用应选择稍后重试、记录失败或其他恢复策略。调度器启动前发送命令时，不会按此参数阻塞等待空间。

调度器运行期间，start/reset 命令携带调用时的 tick，服务任务以该时间加周期计算到期点，而不是从取出命令时才开始完整计时。若取出时已经过期，可能立即执行回调；自动重载还可能补处理多个到期点。

### ISR 提交命令

ISR 使用对应的 `xTimerStartFromISR`、`xTimerResetFromISR`、`xTimerStopFromISR` 或 `xTimerChangePeriodFromISR`，不使用任务版命令 API，也没有等待队列空间的参数；队列满会立即失败，仍需检查返回值。创建和删除没有对应的 FromISR 接口，应交给任务处理。

进入 ISR 时将 `xHigherPriorityTaskWoken` 初始化为 `pdFALSE`，把它的地址传给相关 FromISR API；同一次 ISR 中多个 API 共用该标志时，不要在调用之间清零。若最终为 `pdTRUE`，按 port 约定在退出 ISR 前请求上下文切换，例如 `portYIELD_FROM_ISR`。标志表示需要调度，不表示回调已经执行。调用这些接口的中断还必须满足 port 的系统调用中断优先级限制；回调本身仍是任务上下文，使用任务版 API 和零等待参数。

## One-Shot 和 Auto-Reload

理想调度下的单次与自动重载到期时间线：

 ![One shot vs Auto-reload][2]

- **One-shot**：创建时 `xAutoReload == pdFALSE`。到期后转为 inactive 并调用一次回调，不自动删除对象；再次 start/reset 可重新计时。
- **Auto-reload**：创建时 `xAutoReload == pdTRUE`。每次到期后按原到期点加周期安排下一次，而不是以回调结束时刻重新计时；持续运行直到停止、删除或其他命令改变其行为。

### 积压与有界算例

自动重载定时器不会简单地将服务任务未及时处理的多个周期合并成一次回调。`prvReloadTimer` 会反复推进到期点并调用回调，直到下一到期点晚于本次采样时间；因此可能出现连续回调，进一步延迟命令处理。回调中即使已成功提交 STOP，也要等服务任务处理该命令后才生效，不能保证取消当前这轮补处理。

## 源码依据

- [timers.c：xTimerGenericCommand、prvTimerTask、prvProcessExpiredTimer、prvReloadTimer、prvProcessReceivedCommands](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/timers.c)。
- [include/timers.h：创建、命令 API、FromISR 接口与回调约束](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/timers.h)。

 [1]: ./images/rtos-timer-task-and-timer-command-queue.png
 [2]: ./images/one-shot-timer-behaviour-vs-auto-reload-timer-behaviour.png
