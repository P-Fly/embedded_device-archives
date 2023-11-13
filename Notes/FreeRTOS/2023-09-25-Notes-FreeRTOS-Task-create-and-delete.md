# 任务的创建与删除

**FreeRTOS** 中，创建和删除任务的接口如下：

 - **xTaskCreate**：基础的任务创建接口。

 - **vTaskDelete**：基础的任务删除接口。

 - **xTaskCreateStatic**：使用静态内存创建任务。

 - **xTaskCreateRestricted**：增加了对 **MPU** 的支持。

 - **xTaskCreateRestrictedStatic**：静态版的 **xTaskCreateRestricted**。

其中，**xTaskCreate** 和 **vTaskDelete** 是最基本的两个接口，其它接口只是做了一些参数上的扩展。因此我们只需要重点分析 **xTaskCreate** 和 **vTaskDelete** 的实现即可。

## xTaskCreate

### 功能

创建一个新的任务，并将其添加到准备运行的任务列表中。

### 原型

```
BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
        const char * const pcName,
        const configSTACK_DEPTH_TYPE usStackDepth,
        void * const pvParameters,
        UBaseType_t uxPriority,
        TaskHandle_t * const pxCreatedTask );
```

### 参数

> TaskFunction_t pxTaskCode

提供函数指针，指向任务入口。

> const char * const pcName

指定任务的名称。

> const configSTACK_DEPTH_TYPE usStackDepth

指定栈的深度，实际使用的字节空间为 **usStackDepth \* sizeof(StackType_t)**。

> void * const pvParameters

指定任务参数。

> UBaseType_t uxPriority

指定任务优先级。支持 **MPU** 的系统可以通过设置位 **portPRIVILEGE_BIT** 来选择在 **privileged mode** 下创建任务。

> TaskHandle_t * const pxCreatedTask

返回创建任务的句柄。

### 流程

![xTaskCreate Sequence][1]

 - 申请栈和TCB_t

    内核会分别申请内存空间 **pxNewTCB** 和 **pxStack**。申请顺序是由宏 **portSTACK_GROWTH** 决定，其目的是保证栈不会增长到TCB中。

 - 计算栈顶地址

    对于 **ARM Cortex-M**，需要按照 **8Byte** 对齐，由 **portBYTE_ALIGNMENT** 定义。

    关于栈的约束可以参考文档 *Procedure Call Standard for the ARM Architecture*：

    ![Stack constraints at a public interface][2]

 - 初始化 **TCB_t**

 - 初始化栈空间

    **pxPortInitialiseStack** 负责初始化任务栈，并将最新的栈顶指针赋值给任务 **TCB_t** 中的 **pxTopOfStack**。

    调用该函数后，我们相当于模拟了一次通过 **SVC(Supervisor Call)** 或 **PendSV(Pendable Service Call)** 进入 **Exception** 的过程。

    1. 首先模拟硬件压栈。

        压栈内容可以参考 *ARM®v7-M Architecture Reference Manual*：

        ![Exception entry behavior][3]

    2. 然后压入 **Exception** 的返回值 **portINITIAL_EXC_RETURN**。后面在 **vPortSVCHandler / xPortPendSVHandler** 中会将该值赋予 **R14** 寄存器，并通过 **bx r14** 指令返回 **Thread mode**。

        异常返回值的定义可以参考 *ARM®v7-M Architecture Reference Manual*：

        ![Exception return behavior][4]

    3. 最后调整栈指针，模拟软件压栈 **R11 - R4**。

 - 挂载 **TCB_t**

    **prvAddNewTaskToReadyList** 负责将新创建的任务挂载到等待链表中。并根据新建任务与当前运行任务的优先级的对比结果，确定是否需要进行任务切换。

最终任务创建后，详细的结构图如下：

![xTaskCreate stack view][5]

## vTaskDelete

### 功能

将任务从所有的任务列表和事件列表中删除。

### 原型

```
void vTaskDelete( TaskHandle_t xTaskToDelete );
```

### 参数

> xTaskToDelete

待删除任务的句柄。

### 流程

![vTaskDelete Sequence][6]

 [1]: ./images/xTaskCreate.jpg
 [2]: ./images/stack_constraints.jpg
 [3]: ./images/exception_entry_behavior.jpg
 [4]: ./images/exc_return_definition.jpg
 [5]: ./images/xTaskCreate_stack_view.jpg
 [6]: ./images/vTaskDelete.jpg
