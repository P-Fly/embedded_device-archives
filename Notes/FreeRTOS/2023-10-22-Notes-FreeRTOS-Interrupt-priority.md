# 中断优先级

**FreeRTOS** 对中断优先级的整体要求如下：

![interrupt priority][1]

### User Interrupt Priority

 - 中断优先级值越小，优先级越高。

 - 小于值 **configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY** 的中断由用户自行管理，不应该调用内核的系统函数。

 - 内核只关心 **configLIBRARY_LOWEST_INTERRUPT_PRIORITY** 和 **configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY** 之间的中断，这部分中断可以调用中断安全函数（以 **FromISR** 后缀的系统函数）。

### SVC Exception Priority

 1. 对于带 **MPU** 的 **ARM Cortex-M** 芯片，**SVC Exception** 的优先级由 **portNVIC_SVC_PRI** 定义：

    ```
    #define portNVIC_SVC_PRI    ( ( ( uint32_t ) configMAX_SYSCALL_INTERRUPT_PRIORITY - 1UL ) << 24UL )
    ```

 2. 对于不带 **MPU** 的 **ARM Cortex-M** 芯片，**SVC Exception** 的优先级为 **0**。因为只在启动调度器时，有且只有一次使用该中断，所以内核并不需要初始化 **SVC Exception**。

### PendSV Exception Priority

**PendSV Exception** 必须为最低优先级，因为需要考虑到中断嵌套。如果在多个嵌套中断中同时请求上下文切换，那么应该只执行一次上下文切换程序，并且只有当所有嵌套中断完成时才执行该操作。

### Systick Exception Priority

**FreeRTOS** 建议以最低优先级运行 **Systick Exception**，因为需要为更实时的应用程序预留更高的优先级。

但这不是必须的，有一种观点认为 **Systick Exception** 本身应该具有更高的优先级，因为只有这样才能保证任务切换的实时性。

 [1]: ./images/interrupt_priority.jpg
