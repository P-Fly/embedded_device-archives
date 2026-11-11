# 任务介绍

FreeRTOS 使用任务组织应用，每个任务都有自己的任务控制块、栈和执行上下文。调度器通过保存与恢复上下文，使多个任务交替使用处理器。

## 任务的定义

任务是内核调度的基本执行单元。在常见的共享地址空间模型中，可以将其类比为线程：每个任务有独立的栈和寄存器上下文，程序代码、全局数据及外设资源则可以共享。

多个任务实例可以使用同一个入口函数，分别通过参数和各自的局部变量保存工作状态。创建任务时传入的参数指针按值传递，其指向对象的生命周期及共享访问由应用管理。

任务被切出时，port 保存恢复执行所需的上下文；再次获得 CPU 后，从先前的执行位置继续运行。下图表示任务、上下文与调度器的关系。

![Task Overview][1]

## 实时调度

实时系统关注事件响应和任务执行是否满足截止时间。任务优先级决定就绪任务之间的选择次序，应用还需要分析执行时间、阻塞时间、中断延迟及处理器负载。

调度器实际选择任务时，选取最高有效优先级的就绪任务。任务等待事件或延时时，退出可运行集合，其他就绪任务便有机会运行。

| 配置 | 调度行为 |
| --- | --- |
| `configUSE_PREEMPTION = 1` | 更高优先级任务就绪时，可以触发抢占；临界区、ISR 执行和调度器挂起等情况会影响实际切换时机 |
| `configUSE_PREEMPTION = 0` | 使用协作式调度，当前任务通常在主动让出、阻塞或挂起等调度点交出处理器 |
| 同时启用抢占和 `configUSE_TIME_SLICING = 1` | tick 可触发同优先级就绪任务轮转；各任务实际占用的 CPU 时间仍取决于其执行、阻塞及中断情况 |

关闭时间片时，同优先级任务仍可因主动让出或阻塞发生切换。实时性需要结合这些条件和应用的时序预算验证。

## 主要数据结构

### TCB_t

每个任务都有自己的任务控制块 `TCB_t`（Task Control Block），用于记录上下文位置、链表节点、优先级及各项可选功能的数据。成员布局由内核实现、port 和配置共同决定；应用通过任务句柄及公开 API 操作任务。

| 成员声明 | 用途 | 出现条件 |
| --- | --- | --- |
| `volatile StackType_t *pxTopOfStack;` | 保存任务上下文对应的栈指针，必须为第一成员，供 port 保存与恢复上下文 | 始终存在 |
| `xMPU_SETTINGS xMPUSettings;` | 保存 port 定义的 MPU 设置，存在时为第二成员 | `portUSING_MPU_WRAPPERS = 1` |
| `ListItem_t xStateListItem;` | 用于就绪、延时、挂起及待清理等状态管理链表 | 始终存在 |
| `ListItem_t xEventListItem;` | 用于事件等待链表或 `xPendingReadyList` | 始终存在 |
| `UBaseType_t uxPriority;` | 当前有效优先级，可能已受优先级继承影响；0 为最低优先级 | 始终存在 |
| `StackType_t *pxStack;` | 栈存储区的起始地址，即低地址端 | 始终存在 |
| `char pcTaskName[ configMAX_TASK_NAME_LEN ];` | 调试用任务名称，最多保存长度配置减 1 个字符，保留字符串终止符 | 始终存在 |
| `StackType_t *pxEndOfStack;` | 记录栈高地址端；栈向低地址增长时，保存按 port 要求对齐后的高端位置 | 栈向高地址增长，或 `configRECORD_STACK_HIGH_ADDRESS = 1` |
| `UBaseType_t uxCriticalNesting;` | 任务级临界区嵌套计数 | `portCRITICAL_NESTING_IN_TCB = 1` |
| `UBaseType_t uxTCBNumber;` | 创建时保存全局任务变化计数的快照，供调试追踪使用 | `configUSE_TRACE_FACILITY = 1` |
| `UBaseType_t uxTaskNumber;` | 供应用或追踪工具设置和读取的任务编号 | `configUSE_TRACE_FACILITY = 1` |
| `UBaseType_t uxBasePriority;` | 优先级继承前的基础优先级 | `configUSE_MUTEXES = 1` |
| `UBaseType_t uxMutexesHeld;` | 持有的互斥量数量，参与优先级恢复处理 | `configUSE_MUTEXES = 1` |
| `TaskHookFunction_t pxTaskTag;` | 应用任务标签，可通过 `xTaskCallApplicationTaskHook()` 调用关联函数 | `configUSE_APPLICATION_TASK_TAG = 1` |
| `void *pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ];` | 应用的任务本地指针槽，所指对象的分配、生命周期和释放由应用管理 | `configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0` |
| `configRUN_TIME_COUNTER_TYPE ulRunTimeCounter;` | 按运行统计时钟累计归属于该任务的计数差值，单位由统计时钟决定 | `configGENERATE_RUN_TIME_STATS = 1` |
| `configTLS_BLOCK_TYPE xTLSBlock;` | C 运行库 TLS 块，内核在初始化、切换和清理路径调用对应钩子 | `configUSE_NEWLIB_REENTRANT = 1` 或 `configUSE_C_RUNTIME_TLS_SUPPORT = 1` |
| `volatile uint32_t ulNotifiedValue[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];` | 各通知索引的值 | `configUSE_TASK_NOTIFICATIONS = 1` |
| `volatile uint8_t ucNotifyState[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];` | 各通知索引的状态，与通知值分别维护 | `configUSE_TASK_NOTIFICATIONS = 1` |
| `uint8_t ucStaticallyAllocated;` | 区分栈和 TCB 的静态、动态来源，供删除路径决定释放哪些内存 | `tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0` |
| `uint8_t ucDelayAborted;` | 记录事件等待已被 `xTaskAbortDelay()` 中止，供后续超时检查消费 | `INCLUDE_xTaskAbortDelay = 1` |
| `int iTaskErrno;` | 保存该任务的 errno，切换时与当前上下文的 `FreeRTOS_errno` 交换 | `configUSE_POSIX_ERRNO = 1` |

例如，`portable/RVDS/ARM_CM4F` 使用 port 内的全局临界嵌套计数；是否把计数保存在 TCB 中，应由 `portCRITICAL_NESTING_IN_TCB` 判断。运行统计可能计入任务运行期间的 ISR 耗时，具体取决于统计时钟及采集方式。互斥量的优先级继承用于减轻优先级反转，实时预算还需考虑持锁时间和多锁依赖。

下图展示栈向低地址和高地址增长时的地址关系。`pxTopOfStack` 表示最近保存的上下文位置；任务正在运行时，实际 SP 由处理器维护。`pxEndOfStack` 是否保存在 TCB 中取决于上表的条件。

![Stack Instance][2]

## 主要全局变量

| 变量声明 | 用途 |
| --- | --- |
| `portDONT_DISCARD PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;` | 调度器启动后指向当前任务的 TCB |
| `PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];` | 按优先级组织可运行任务，当前运行任务的状态项通常也留在对应就绪链表中 |
| `PRIVILEGED_DATA static List_t xDelayedTaskList1;` | 两个延时链表的存储实体之一 |
| `PRIVILEGED_DATA static List_t xDelayedTaskList2;` | 两个延时链表的另一个存储实体 |
| `PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;` | 当前 tick 周期的延时链表，存放唤醒 tick 未发生无符号回绕的任务 |
| `PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;` | 溢出延时链表，存放唤醒 tick 已发生无符号回绕的任务；tick 回绕时交换两个指针 |
| `PRIVILEGED_DATA static List_t xPendingReadyList;` | 调度器挂起期间，通过事件项暂存待就绪任务，最外层恢复时迁移其状态项 |
| `PRIVILEGED_DATA static List_t xTasksWaitingTermination;` | 自删除任务等待 Idle 清理的链表，受 `INCLUDE_vTaskDelete` 控制 |
| `PRIVILEGED_DATA static List_t xSuspendedTaskList;` | 保存显式挂起和符合无限期等待条件的任务，受 `INCLUDE_vTaskSuspend` 控制 |
| `int FreeRTOS_errno = 0;` | `configUSE_POSIX_ERRNO = 1` 时当前上下文使用的 errno，与各任务的 `iTaskErrno` 配合保存、恢复 |
| `PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;` | 当前内核管理的任务数，包含运行、就绪、阻塞、挂起及尚未被 Idle 清理的自删除任务 |
| `PRIVILEGED_DATA static volatile TickType_t xTickCount = ( TickType_t ) configINITIAL_TICK_COUNT;` | 内核的 tick 计数 |
| `PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;` | 就绪优先级的搜索记录，具体表示由任务选择方式决定 |
| `PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning = pdFALSE;` | 调度器已启动且未结束的标志，挂起调度器期间仍保持原值 |
| `PRIVILEGED_DATA static volatile TickType_t xPendedTicks = ( TickType_t ) 0U;` | 调度器挂起期间积累、等待恢复时处理的 tick 数 |
| `PRIVILEGED_DATA static volatile BaseType_t xYieldPending = pdFALSE;` | 待处理的任务切换请求 |
| `PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows = ( BaseType_t ) 0;` | tick 计数回绕次数，参与跨回绕的超时判断 |
| `PRIVILEGED_DATA static UBaseType_t uxTaskNumber = ( UBaseType_t ) 0U;` | 创建或删除任务时递增的变化计数，供调试器检测任务集合变化 |
| `PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime = ( TickType_t ) 0U;` | 当前延时链表的最早唤醒 tick；空表时使用 `portMAX_DELAY` |
| `PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle = NULL;` | Idle 任务句柄 |
| `PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended = ( UBaseType_t ) pdFALSE;` | 调度器挂起的嵌套计数，减到零时才完成最外层恢复 |
| `PRIVILEGED_DATA static configRUN_TIME_COUNTER_TYPE ulTaskSwitchedInTime = 0UL;` | 启用运行统计时，保存任务最近切入时的统计时钟读数 |
| `PRIVILEGED_DATA static volatile configRUN_TIME_COUNTER_TYPE ulTotalRunTime = 0UL;` | 启用运行统计时，保存最近切换时读取的统计时钟值 |

通用任务选择方式使用数值记录就绪优先级的搜索起点。`portable/RVDS/ARM_CM4F` 在 `configUSE_PORT_OPTIMISED_TASK_SELECTION = 1` 时使用位图，要求 `configMAX_PRIORITIES` 不超过 32；表示方法由 port 的选择宏决定。

全局 `uxTaskNumber` 用于追踪任务集合变化，TCB 的 `uxTCBNumber` 在创建时取得该计数的快照。自删除任务由 Idle 摘除待清理项时才递减 `uxCurrentNumberOfTasks`，应用动态申请的资源由应用负责释放。

## 任务的状态

### 状态描述

`eTaskState` 描述任务在查询时的状态：

| 状态 | 含义 |
| --- | --- |
| `eRunning` | 当前任务，由 `pxCurrentTCB` 标识；执行 ISR 时，处理器暂时执行异常处理代码 |
| `eReady` | 具备运行条件，等待调度器选择 |
| `eBlocked` | 等待时间、事件或通知，可在满足条件、超时或被中止等待时解除阻塞 |
| `eSuspended` | 被显式挂起，需要通过任务恢复接口恢复 |
| `eDeleted` | 已进入删除路径，可能仍有内核资源等待清理 |

启用 `INCLUDE_eTaskGetState = 1` 后，可以通过 `eTaskGetState()` 查询仍有效的任务句柄。结果是瞬时观察，后续可能立即改变；任务资源释放后，句柄随之失效，生命周期应由应用管理。

### 任务的状态和全局链表的关系

链表记录内部管理关系，状态查询还需要结合当前任务、事件项和通知状态判断。

- 当前运行任务通常仍在就绪链表中，`eTaskGetState()` 先通过 `pxCurrentTCB` 将它识别为 `eRunning`。
- 状态项位于两个延时链表之一的任务，被识别为 `eBlocked`。队列等待可因事件或超时解除阻塞，纯延时则主要等待唤醒 tick。
- 支持无限期阻塞的接口，在 `INCLUDE_vTaskSuspend = 1` 且等待时间为 `portMAX_DELAY` 时，可以把状态项放入 `xSuspendedTaskList`。例如，队列接收可以无限等待消息；`vTaskDelay(portMAX_DELAY)` 则仍表示有限 tick 延时。
- 对于挂起链表中的任务，查询还会检查事件项归属及各通知索引是否正在等待；事件或通知等待中的任务返回 `eBlocked`，显式挂起的任务返回 `eSuspended`。
- 调度器挂起期间，事件项进入 `xPendingReadyList` 后，状态项暂留原链表。恢复迁移前，`eTaskGetState()` 仍可能返回 `eBlocked`；最外层恢复后再参与正常调度。
- 启用任务删除功能时，待清理链表中的任务以及状态项容器为空的相应删除路径可返回 `eDeleted`，查询要求 TCB 仍然有效。

### 状态迁移

| 起始状态 | 典型触发条件 | 结果 |
| --- | --- | --- |
| 创建任务 | 创建成功并加入就绪链表 | Ready；调度器已启动时可能随即获得 CPU |
| Ready | 调度器选中该任务 | Running |
| Running | 被抢占或主动让出，且仍具备运行条件 | Ready；主动让出后也可能再次选中自身 |
| Running | 等待尚未满足的事件、通知或非零延时 | Blocked |
| Blocked | 事件到达、等待超时或 `xTaskAbortDelay()` 成功 | Ready，随后按调度条件运行 |
| Running / Ready / Blocked | 调用 `vTaskSuspend()` 挂起对应任务 | Suspended |
| Suspended | 调用 `vTaskResume()` 或 `xTaskResumeFromISR()` | Ready，必要时延后就绪处理 |
| 仍存在的任务 | 调用 `vTaskDelete()` | Deleted；自删除的内核资源交由 Idle 清理 |

![Stack Transition][3]

## 源码依据

- [tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：`TCB_t`、任务管理变量、`prvInitialiseNewTask()`、`vTaskSwitchContext()`、`eTaskGetState()`、`vTaskDelete()`。
- [task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：任务状态枚举及任务管理 API 的使用条件。
- [FreeRTOS.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/FreeRTOS.h)：可选功能默认值、分配条件及运行库 TLS 钩子。
- [Cortex-M4F portmacro.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/portmacro.h)：优化任务选择的位图和优先级数量限制。

 [1]: ./images/task_overview.jpg
 [2]: ./images/stack_instance.jpg
 [3]: ./images/task_states.jpg
