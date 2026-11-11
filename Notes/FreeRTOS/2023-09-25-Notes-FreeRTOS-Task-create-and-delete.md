# 任务的创建与删除

创建任务时，需要为任务栈和 TCB 准备存储，并初始化上下文及调度信息。不同接口的内存来源和使用条件如下：

| 创建接口 | 栈来源 | TCB 来源 | 使用条件 |
| --- | --- | --- | --- |
| `xTaskCreate` | 内核动态分配 | 内核动态分配 | `configSUPPORT_DYNAMIC_ALLOCATION = 1` |
| `xTaskCreateStatic` | 调用方提供 | 调用方提供 | `configSUPPORT_STATIC_ALLOCATION = 1` |
| `xTaskCreateRestricted` | 调用方提供 | 内核动态分配 | 使用 MPU wrappers 的 port，并启用动态分配 |
| `xTaskCreateRestrictedStatic` | 调用方提供 | 调用方提供 | 使用 MPU wrappers 的 port，并启用静态分配 |

这些接口共享部分初始化和加入就绪链表的流程。Restricted 接口通过 `TaskParameters_t` 配置任务及其 MPU 内存区域，具体权限和区域约束由 MPU port 决定。

`xTaskCreateStatic()` 成功时直接返回任务句柄；其他三个创建接口返回状态码，并通过输出参数提供句柄。调用方提供的栈和 TCB 缓冲区，在任务使用及内核清理完成前必须保持有效。

任务删除使用 `vTaskDelete()`，由 `INCLUDE_vTaskDelete = 1` 启用。下面重点说明动态创建及删除流程。

## xTaskCreate

### 功能

动态分配任务栈和 TCB，初始化任务后，将其加入对应优先级的就绪链表。调度器已运行且启用抢占时，新建的更高优先级任务可能在创建接口返回前开始执行。

### 原型

```c
BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
        const char * const pcName,
        const configSTACK_DEPTH_TYPE usStackDepth,
        void * const pvParameters,
        UBaseType_t uxPriority,
        TaskHandle_t * const pxCreatedTask );
```

### 参数

`TaskFunction_t pxTaskCode`

任务入口函数。任务函数应持续执行或在完成工作、清理资源后自删除，不能正常返回。

`const char * const pcName`

调试用任务名称，内核复制到 TCB 的名称数组中，长度受 `configMAX_TASK_NAME_LEN` 限制。

`const configSTACK_DEPTH_TYPE usStackDepth`

栈深度以 `StackType_t` 元素数为单位，所需存储字节数为 `usStackDepth * sizeof(StackType_t)`。例如，32 位 `StackType_t` 下，256 个元素需要 1024 B。

`void * const pvParameters`

传给任务入口的参数指针，可为 `NULL`。内核传递指针值，其指向对象必须在任务使用期间保持有效。

`UBaseType_t uxPriority`

任务优先级，有效范围为 `0` 到 `configMAX_PRIORITIES - 1`。使用 MPU wrappers 的 port 时，可通过 `portPRIVILEGE_BIT` 选择特权任务；访问权限还取决于该 port 的配置。

`TaskHandle_t * const pxCreatedTask`

可选的句柄输出参数。不需要获取句柄时可传 `NULL`；需要句柄时，应在确认创建成功后使用输出值。

### 返回值

- `pdPASS`：创建成功，任务已加入就绪链表。
- `errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY`：栈或 TCB 分配失败，任务未创建；已分配的部分由创建路径释放。

调用前应确保参数有效、栈深度足够，并处理分配失败的返回值。

### 流程

动态创建的主要调用关系如下：`xTaskCreate()` 分配存储，`prvInitialiseNewTask()` 初始化任务，最后由 `prvAddNewTaskToReadyList()` 加入就绪链表。

![xTaskCreate Sequence][1]

#### 分配栈和 TCB

内核分别为 `pxNewTCB` 和 `pxStack` 分配空间。`portSTACK_GROWTH < 0` 时先分配栈，再分配 TCB；向高地址增长时顺序相反。如果第二次分配失败，会释放第一次分配的空间。

这种分配顺序旨在降低栈向 TCB 方向增长的风险。实际地址关系取决于堆实现和空闲块分布，任务仍需要足够的栈空间及相应的溢出检测或保护。

#### 计算初始栈顶

以 `portable/RVDS/ARM_CM4F` 为例，`StackType_t` 为 32 位，栈向低地址增长，`portBYTE_ALIGNMENT` 为 8。内核从栈数组的高地址端计算初始位置，再向低地址按 8 B 边界对齐。

AAPCS32 区分持续成立的栈约束与公共接口约束：SP 始终满足 4 B 对齐，在公共接口处满足 8 B 对齐。下图摘录了这两项约束；任务初始上下文的安排还需满足所选 port 的恢复流程。

![Stack constraints at a public interface][2]

#### 初始化 TCB

`prvInitialiseNewTask()` 设置优先级、名称、栈存储区地址和链表节点，并按配置初始化通知、运行统计等可选成员；随后调用 port 的栈初始化函数。

#### 构造初始上下文

`pxPortInitialiseStack()` 按首次恢复任务所需的格式写入和预留栈槽，并将返回的上下文指针保存到 TCB 的 `pxTopOfStack`。创建期间通过内存操作构造这些槽位，首次运行时由 SVC 恢复寄存器并执行异常返回。

Armv7-M 的基本异常帧包含 R0、R1、R2、R3、R12、LR、PC 和 xPSR。下图摘录了 *Armv7-M Architecture Reference Manual* 的 B1.5.6 异常入口行为：

![Exception entry behavior][3]

对于上述无 MPU 的 Cortex-M4F port，以下偏移从 `pxPortInitialiseStack()` 返回的指针开始计算，按地址从低到高排列，每个槽位为 4 B：

| 字节偏移 | 内容 | 初始用途 |
| --- | --- | --- |
| 0–28 | R4–R11 | 软件恢复的寄存器槽，初始化函数预留空间 |
| 32 | EXC_RETURN | `portINITIAL_EXC_RETURN = 0xFFFFFFFD` |
| 36 | R0 | 任务参数 `pvParameters` |
| 40–52 | R1、R2、R3、R12 | 基本异常帧中的寄存器槽，初始化函数预留空间 |
| 56 | LR，即任务的 R14 | `prvTaskExitError`，处理任务意外返回 |
| 60 | PC | 任务入口地址，按 port 要求处理地址位 |
| 64 | xPSR | `0x01000000`，设置 Thumb 状态位 |

SVC 恢复处理先读取 R4–R11 和 EXC_RETURN，再把 PSP 设置到表中 R0 的位置。此时处理函数的 R14 保存 EXC_RETURN，执行 `bx r14` 后，硬件恢复基本帧并进入任务入口；基本帧中的 LR 则成为任务运行时的返回地址。

`0xFFFFFFFD` 表示返回 Thread mode、使用 PSP、采用基本异常帧。异常返回规则可参考下图摘录的架构手册 B1.5.8：

![Exception return behavior][4]

该初始上下文未包含浮点扩展帧。任务使用 FPU 后，后续切换会按 EXC_RETURN 的位 4 判断是否需要额外保存和恢复 S16–S31，硬件浮点帧的处理还受浮点上下文及延迟保存机制影响。MPU port 的附加上下文应以其初始化和切换实现为准。

下图给出向低地址增长的初始栈地址示例。`pxTopOfStack` 最终指向软件恢复块起点，具体地址和栈与 TCB 的相对位置取决于实际分配结果。

![xTaskCreate stack view][5]

#### 加入就绪链表

`prvAddNewTaskToReadyList()` 更新任务管理信息，并将状态项加入对应优先级的就绪链表。调度器尚未启动时，任务等待启动后被选中；调度器已运行时，是否立即请求切换由新任务优先级和抢占配置决定。

## vTaskDelete

### 功能

移除任务的状态项及已挂接的事件项，结束其调度生命周期，并根据是否为当前任务及内存来源清理内核资源。该接口在任务上下文调用。

### 原型

```c
void vTaskDelete( TaskHandle_t xTaskToDelete );
```

### 参数

`xTaskToDelete`

待删除任务的有效句柄；传入 `NULL` 表示删除调用任务自身。删除后应用停止使用该句柄，并协调其他任务对相关资源的访问。

### 流程

下图表示调度器已运行时，删除当前任务与删除其他任务的主要分支：

![vTaskDelete Sequence][6]

1. 从任务所在的状态链表移除状态项；若事件项仍挂在链表中，也将其移除。
2. 如果删除的是当前任务，将其放入 `xTasksWaitingTermination`，请求切换到其他任务。Idle 随后通过 `prvCheckTasksWaitingTermination()` 和 `prvDeleteTCB()` 完成清理。
3. 如果删除的是其他任务，在删除调用路径中执行 `prvDeleteTCB()`，回收应由内核释放的存储，并更新相关任务管理信息。

调度器挂起期间不能自删除。自删除后 Idle 需要获得运行机会，才能完成延后清理；如果 Idle 长期不能运行，待清理任务的内存会持续占用。

### 内存与资源归属

| 创建方式 | 删除时的内存处理 |
| --- | --- |
| `xTaskCreate` | 内核释放动态分配的栈和 TCB |
| `xTaskCreateRestricted` | 内核释放动态 TCB，调用方提供的栈由调用方管理 |
| `xTaskCreateStatic` / `xTaskCreateRestrictedStatic` | 栈和 TCB 均由调用方管理，内核完成对应清理流程 |

调用方提供的存储应在任务退出且内核清理完成后再复用；自删除场景需要等待 Idle 完成清理。任务自行申请的缓冲区、持有的互斥量及外设资源由应用负责释放或移交，自删除前应完成必要的资源清理。外部删除也需要与目标任务建立安全的退出协议，避免其正处于资源使用过程中。

## 源码依据

- [tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：各创建接口、`prvInitialiseNewTask()`、`prvAddNewTaskToReadyList()`、`vTaskDelete()`、`prvCheckTasksWaitingTermination()`、`prvDeleteTCB()`。
- [task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：创建和删除接口的参数、返回值及使用限制。
- [Cortex-M4F port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/port.c)：`pxPortInitialiseStack()`、`vPortSVCHandler()` 和 `xPortPendSVHandler()`。
- [Cortex-M4F portmacro.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/portmacro.h)：栈元素类型、增长方向及对齐配置。

 [1]: ./images/xTaskCreate.jpg
 [2]: ./images/stack_constraints.jpg
 [3]: ./images/exception_entry_behavior.jpg
 [4]: ./images/exc_return_definition.jpg
 [5]: ./images/xTaskCreate_stack_view.jpg
 [6]: ./images/vTaskDelete.jpg
