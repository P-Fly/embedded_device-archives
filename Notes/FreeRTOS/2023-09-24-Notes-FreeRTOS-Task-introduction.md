# 任务介绍

**FreeRTOS** 是一个轻量级实时操作系统。它由一个个独立的 **任务** 组成，而每个 **任务** 都有自己独立的 **数据结构** 和 **代码**。因此我们将从全局的角度出发对 **任务** 进行描述。

## 任务的定义

在维基百科中，对 **Task** 的定义如下：

> In computing, a task is a unit of execution or a unit of work.
> ...
> In the sense of "unit of execution", in some operating systems, a task is synonymous with a process, and in others with a thread.
> ...

**Task** 就是我们平时在程序中使用的 **Process** 或 **Thread**。当然，由于嵌入式系统与我们平时使用的 **PC** 系统相比，硬件结构要简单的多，编程模型也相对来说比较简单。因此，在 **FreeRTOS** 中没有对 **Process** 和 **Thread** 进行区分，而是统一使用 **Task** 作为基本的执行单元。

在此，我们引用 **FreeRTOS** 官网上的话来描述：

> A real time application that uses an RTOS can be structured as a set of independent tasks. Each task executes within its own context with no coincidental dependency on other tasks within the system or the RTOS scheduler itself. Only one task within the application can be executing at any point in time and the real time RTOS scheduler is responsible for deciding which task this should be. The RTOS scheduler may therefore repeatedly start and stop each task (swap each task in and out) as the application executes.

![Task Overview][1]

## 主要数据结构

### TCB_t

**FreeRTOS** 的核心是 **任务管理**，每个任务都有一些数据需要存储。**FreeRTOS** 把这些数据集中到一个数据结构中进行管理，这个数据结构就是结构体 **TCB_t**，被称作 **Task Control Block**。

每个任务都有各自独立的 **TCB_t**，一旦任务被建立，该结构体将被赋值。当任务处于非运行状态时，**FreeRTOS** 会用该结构体来保存任务的状态。当任务重新运行时，该结构体能确保任务从被中断的位置继续执行。

**TCB_t** 的成员如下：

> volatile StackType_t *pxTopOfStack;

指向 **栈顶** 的指针，该成员变量必须放在数据结构第一个元素的位置。在汇编中会利用该指针进行压栈和出栈操作，比如在 **xPortPendSVHandler** 中的实现。

> xMPU_SETTINGS xMPUSettings;

内存保护单元的配置项，记录了各个内存段的基地址和访问属性。

> ListItem_t xStateListItem;

**状态链表** 的节点项，内核根据任务所处的不同状态，将任务插入到对应的 **状态链表** 中。

> ListItem_t xEventListItem;

**事件链表** 的节点项，内核根据任务所依赖的不同事件，将其插入到对应的 **事件链表** 中。

> UBaseType_t uxPriority;

任务的优先级，**0为最低优先级**。

> StackType_t *pxStack;

指向 **栈空间** 的起始地址。

> char pcTaskName[ configMAX_TASK_NAME_LEN ];

任务的名称。

> StackType_t *pxEndOfStack;

指向 **栈空间** 的结束地址。

下图展示了 **pxTopOfStack**，**pxEndOfStack**，**pxStack** 之间的关系。

![Stack Instance][2]

> UBaseType_t uxCriticalNesting;

用于临界区嵌套计数。**ARM Cortex-M** 不使用该变量，取而代之的是一个 **全局的临界区嵌套计数**。

> UBaseType_t uxTCBNumber;

调试追踪时使用，存储一个每次创建 **TCB** 时递增的数字。

> UBaseType_t uxTaskNumber;

调试追踪时使用，用户可以自定义设置该值。**FreeRTOS** 不使用该值。

> UBaseType_t uxBasePriority;

> UBaseType_t uxMutexesHeld;

在 **互斥锁** 中使用的 **优先级继承机制**，能解决 **优先级反转** 的问题。

> TaskHookFunction_t pxTaskTag;

允许用户设置一个回调函数的指针。**FreeRTOS** 不使用该指针。

> void *pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ];

允许用户存储一些本地数据，**FreeRTOS** 不使用该指针。

> configRUN_TIME_COUNTER_TYPE ulRunTimeCounter;

记录任务运行的总时间。

> configTLS_BLOCK_TYPE xTLSBlock;

提供对 **newlib** 的支持，**FreeRTOS** 不使用该值。

> volatile uint32_t ulNotifiedValue[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];

> volatile uint8_t ucNotifyState[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];

提供 **任务通知** 机制。

> uint8_t ucStaticallyAllocated;

标识 **栈空间** 和 **TCB** 是 **静态内存** 还是 **动态内存**。在 **vTaskDelete** 中根据该状态字进行相应的注销操作。

> uint8_t ucDelayAborted;

标识该任务是否需要 **立即唤醒**，可以使用 **xTaskAbortDelay** 停止延时等待。

> int iTaskErrno;

记录该任务的 **errno**。

## 主要全局变量

### pxCurrentTCB

> portDONT_DISCARD PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;

指向当前正在运行的任务。任何时刻，都只有一个任务处于 **运行态**。

### 等待链表

> PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];

按照 **优先级** 记录当前处于 **就绪态** 的任务，每个链表头代表一组 **相同优先级** 的任务。

### 延时链表

> PRIVILEGED_DATA static List_t xDelayedTaskList1;

> PRIVILEGED_DATA static List_t xDelayedTaskList2;

> PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;

> PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;

 - **pxDelayedTaskList**：指向 **延时链表**。如果唤醒时间 **xTickCount + xTicksToDelay** 未溢出，则将任务放入到该链表中。

 - **pxOverflowDelayedTaskList**：指向 **溢出延时链表**。如果唤醒时间 **xTickCount + xTicksToDelay** 会产生溢出，则将任务放入到该链表中。

### PendingReady链表

> PRIVILEGED_DATA static List_t xPendingReadyList;

在 **调度器** 挂起期间，若 **ISR** 需要让一个任务进入 **就绪态**，此时，不能将该任务直接放入 **pxReadyTasksLists** 链表中，而是需要暂时存放到 **xPendingReadyList** 链表中。该链表使用 **pxTCB->xEventListItem** 作为链表项，所以也可以认为它是一个 **消息链表**。

一旦 **调度器** 恢复运行，会立即把 **xPendingReadyList** 链表中的所有任务移动到合适的 **pxReadyTasksLists** 链表中。

### WaitingTermination链表

> PRIVILEGED_DATA static List_t xTasksWaitingTermination;

当前正在运行的任务如果需要被删除，无法立即释放资源。此时需要使用该链表进行记录，以便后续在 **IDLE** 任务中进行释放操作。

### Suspended链表

> PRIVILEGED_DATA static List_t xSuspendedTaskList;

跟踪被 **挂起** 的任务，即跟踪 **与时间无关** 的任务。

### 其它变量

> int FreeRTOS_errno = 0;

当前任务的错误字。

> PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;

当前管理的任务数，包括所有就绪，阻塞，挂起和待删除的任务。

> PRIVILEGED_DATA static volatile TickType_t xTickCount = ( TickType_t ) configINITIAL_TICK_COUNT;

内核的 **滴答计数**。

> PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;

在 **ARM Cortex-M** 中以 **Bit** 位记录当前哪些优先级被使用，能通过该变量快速找出最高优先级。

> PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning = pdFALSE;

记录 **调度器** 是否处于运行状态。

> PRIVILEGED_DATA static volatile TickType_t xPendedTicks = ( TickType_t ) 0U;

当使用 **vTaskSuspendAll** 挂起调度器时，该变量跟踪这期间未被处理的内核滴答数。

> PRIVILEGED_DATA static volatile BaseType_t xYieldPending = pdFALSE;

记录 **调度器** 是否需要重新调度。

> PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows = ( BaseType_t ) 0;

记录 **定时器** 溢出次数。

> PRIVILEGED_DATA static UBaseType_t uxTaskNumber = ( UBaseType_t ) 0U;

记录全局任务数，每个任务会被分配到一个唯一的 **Number**。

> PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime = ( TickType_t ) 0U;

记录 **延时链表** 中，下一次需要唤醒的时间点。

> PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle = NULL;

**IDLE** 任务的句柄。

> PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended = ( UBaseType_t ) pdFALSE;

记录 **调度器** 是否被挂起。

> PRIVILEGED_DATA static configRUN_TIME_COUNTER_TYPE ulTaskSwitchedInTime = 0UL;

记录上次任务切换时的时间。

> PRIVILEGED_DATA static volatile configRUN_TIME_COUNTER_TYPE ulTotalRunTime = 0UL;

记录总的运行时间。

## 任务的状态

### 状态描述

**FreeRTOS** 从用户的维度定义了一个枚举 **eTaskState**，该枚举描述了任务的状态：

 - **Running**：

    - 当前 **正在执行** 的任务，始终由 **pxCurrentTCB** 所指向。

 - **Ready**：

    - 属于 **pxReadyTasksLists** 链表的任务。

    - 被挂载到消息链表 **xPendingReadyList** 上的任务。注意此时任务的状态项 **xStateListItem** 可能依然处于其它链表上。

 - **Blocked**：

    - 属于 **pxDelayedTaskList** 链表的任务，该组任务可以被 **超时唤醒**，也可以被 **事件唤醒**。

    - 属于 **pxOverflowDelayedTaskList** 链表的任务，该组任务可以被 **超时唤醒**，也可以被 **事件唤醒**。

    - 属于 **xSuspendedTaskList** 链表，但正在等待 **事件唤醒** 的任务。

    - 属于 **xSuspendedTaskList** 链表，但正在等待 **任务通知** 的任务。

 - **Suspended**：

    - 属于 **xSuspendedTaskList** 链表，并且没有等待 **事件唤醒** 和 **任务通知** 的任务。

 - **Deleted**：

    - 属于 **xTasksWaitingTermination** 链表的任务。

    - 不属于任何链表的任务。

### 任务的状态和全局链表的关系

需要注意 **任务的状态** 和 **全局链表** 是程序的两个维度，是不能直接对应的。两者的差异可以从函数 **eTaskGetState** 的处理逻辑中看出：

 - **任务的状态** 是从用户的维度来看待任务，对应枚举值 **eTaskState**，其中：

    - **Blocked** 是指会在一定条件下被内核唤醒的任务，比如 **超时唤醒**，**事件唤醒**，**任务通知** 等。

    - **Suspended** 是指永远不会被内核自动唤醒的任务。

 - **全局链表** 是从内核的维度来看待任务，其中：

    - 如果任务会在一段时间后被唤醒，就会挂载到链表 **pxDelayedTaskList** 或 **pxOverflowDelayedTaskList** 上。比如：

        - 使用 **xQueueReceive** 等待消息接收，超时时间不为 **portMAX_DELAY**。

        - 使用 **vTaskDelay** 等待一段时间被唤醒。

    - 如果任务不会被时间唤醒，就会挂载到链表 **xSuspendedTaskList** 上，比如：

        - 使用 **xQueueReceive** 等待消息接收，超时时间为 **portMAX_DELAY**。

        - 使用 **vTaskSuspend** 挂起任务。

### 状态迁移

![Task States][3]

 [1]: ./images/task_overview.jpg
 [2]: ./images/stack_instance.jpg
 [3]: ./images/task_states.jpg
