# 调度器的启动

## vTaskStartScheduler

### 原型

```
void vTaskStartScheduler( void );
```

### 流程

![vTaskStartScheduler][1]

 - 创建 **IDLE** 和 **Tmr Svc** 任务

 - 关闭中断

    对于 **ARM Cortex-M**，开关中断的代码实现如下：

    ```
    #define portDISABLE_INTERRUPTS()                  vPortRaiseBASEPRI()
    #define portENABLE_INTERRUPTS()                   vPortSetBASEPRI( 0 )

    static portFORCE_INLINE void vPortRaiseBASEPRI( void )
    {
        uint32_t ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;

        __asm
        {
            /* Set BASEPRI to the max syscall priority to effect a critical
             * section. */
            msr basepri, ulNewBASEPRI
            dsb
            isb
        }
    }

    static portFORCE_INLINE void vPortSetBASEPRI( uint32_t ulBASEPRI )
    {
        __asm
        {
            /* Barrier instructions are not used as this function is only used to
             * lower the BASEPRI value. */
            msr basepri, ulBASEPRI
        }
    }
    ```

    通过设置 **BASEPRI** 寄存器的方式来开关中断，该寄存器属于特殊寄存器，使用 **msr / mrs** 指令进行操作。该寄存器的用途如下：

    1. 当 **BASEPRI** 被设成某个值后，所有优先级号大于等于此值的硬件中断都被关闭 (优先级号越大，优先级越低)。

    2. 当 **BASEPRI = 0** 时，不屏蔽任何中断。

        ![basepri][2]

    对于 **ARM Cortex-M** 来说，优先级号越小，则优先级越高。芯片支持中断嵌套，使得 **高优先级异常会抢占低优先级异常**。同时有 **3** 个系统异常：**Reset**，**NMI** 和 **HardFault**，它们有固定的 **负数优先级号**，从而高于其它所有异常。其它异常的优先级则都是可编程的，并且为了提高程序的可移植性，优先级号是按照 **MSB** 对齐的方式定义的。

    ![priority value][3]

 - 校验中断优先级

    内核对中断优先级的要求如下：

    ![interrupt priority][4]

 - 配置 **PendSV** 和 **SysTick** 的中断优先级

    默认优先级为 **configLIBRARY_LOWEST_INTERRUPT_PRIORITY**：

    ```
    portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
    portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;
    ```

 - 配置 **Systick**

 - **prvStartFirstTask**

    ```
    PRESERVE8
    ````

    声明当前函数的栈需要 **8Byte** 对齐。

    ```
    ldr r0, =0xE000ED08
    ldr r0, [r0]
    ldr r0, [r0]
    msr msp, r0
    ```

    根据 **VTOR** 寄存器获取向量表的地址，并将第一组 **32Bit** 数据赋予 **MSP** 寄存器。

    ![vtor register][5]

    1. 根据 **ARM Cortex-M** 的定义，向量表的第一个 **32Bit** 数据为系统上电时的默认 **栈地址**。因此这组操作相当于 **删除了从代码启动到当前位置之间所记录的栈内容**。

    2. 这段栈的空间一般在启动文件中有明确的定义，比如 **Keil** 在 **Startup.S** 文件中定义的 **Stack_Size**。

    3. 根据后面任务切换的分析，我们可以总结出不同栈的用途：

        - 启动文件定义的 **系统栈** 用于中断上下文，使用 **MSP** 进行访问。

        - 各个任务定义的任务栈由 **TCB_t** 去追踪，使用 **PSP** 进行访问。

    ```
    mov r0, #0
    msr control, r0
    ```

    清除 **control** 寄存器，该寄存器的主要功能定义如下：

    1. 定义在 **Thread mode** 下的执行权限 **privileged / unprivileged**。

    2. 定义要使用的 **SP** 寄存器。

    3. 定义 **浮点扩展帧** 是否在当前上下文中被激活。

    ```
    cpsie i
    cpsie f
    dsb
    isb
    ```

    使能全局中断，并清理缓存。此时虽然全局中断已经打开，但 **BASEPRI** 寄存器依然被设为 **configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY**，只有 **用户自行管理的中断** 和 **SVC Exception** 可以被执行。

    ```
    svc 0
    nop
    nop
    ```

    手动产生 **SVC Exception**。本例中 **Thread mode** 支持 **privileged access**，因此 **SVC Exception** 只在内核启动时才会被使用，也因此不需要配置该中断的优先级。

 - **vPortSVCHandler**

    硬件首先会将8个寄存器自动压入栈空间。因为异常进入前使用的栈指针为 **MSP**，因此该部分寄存器存储在 **系统栈** 中。

    ```
    ldr r3, =pxCurrentTCB
    ldr r1, [r3]
    ldr r0, [r1]
    ```

    通过 **pxCurrentTCB** 找到下一个需要执行的任务的栈。

    ```
    ldmia r0!, {r4-r11, r14}
    ```

    将 **r4 - r11** 手动出栈。此时 **r14** 寄存器恢复的数据在入栈时被设置为了 **0xFFFFFFFD**，表示退出 **SVC Exception** 时模式切换为 **Thread mode**，栈指针切换为 **PSP**。

    ![exception return behavior][6]

    ```
    msr psp, r0
    isb
    ```

    在退出前手动调整栈指针，相当于 **psp = pxCurrentTCB->pxTopOfStack**。

    ```
    mov r0, #0
    msr basepri, r0
    ```

    取消优先级屏蔽，此时所有中断均可以被响应。

    ```
    bx r14
    ```

    退出 **SVC Exception** 模式，进入 **Thread mode**。该异常退出时硬件使用 **PSP** 寄存器出栈，后续在任务中也继续使用 **PSP** 寄存器。

 [1]: ./images/vTaskStartScheduler.jpg
 [2]: ./images/basepri.jpg
 [3]: ./images/priority_value.jpg
 [4]: ./images/interrupt_priority.jpg
 [5]: ./images/vtor_register.jpg
 [6]: ./images/exc_return_definition.jpg
