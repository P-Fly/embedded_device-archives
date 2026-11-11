# 调度器的启动

## vTaskStartScheduler

### 功能

启动内核调度器，使就绪任务开始按调度规则运行。应用应在启动前完成必要的硬件初始化和任务创建，并从主程序的 Thread mode 调用此接口。

以下寄存器操作和汇编对应 `portable/RVDS/ARM_CM4F`：使用 Cortex-M4F、非 MPU port 和 Arm Compiler 5 的汇编语法，并要求编译目标启用硬件浮点。启动入口应处于特权 Thread mode，SVC、PendSV 和 SysTick 的异常入口应连接到对应 port 处理函数。

### 原型

```c
void vTaskStartScheduler( void );
```

### 流程

| 阶段 | 主要工作 |
| --- | --- |
| `vTaskStartScheduler()` | 创建 Idle 任务；启用软件定时器时创建定时器服务任务；设置中断屏蔽和内核运行状态 |
| `xPortStartScheduler()` | 检查优先级配置，设置 PendSV/SysTick 优先级，初始化节拍中断、临界区计数和浮点支持 |
| `prvStartFirstTask()` | 从向量表恢复 MSP，设置 CONTROL 和中断屏蔽状态，通过 SVC 进入首次上下文恢复 |
| `vPortSVCHandler()` | 恢复当前任务的软件保存块，设置 PSP，通过异常返回进入任务入口 |

#### 创建内核任务

Idle 任务始终需要创建。当 `configSUPPORT_STATIC_ALLOCATION == 1` 时，通过 `vApplicationGetIdleTaskMemory()` 获取应用提供的 TCB 和栈；否则使用动态分配。只有 Idle 创建成功，且 `configUSE_TIMERS == 1` 时，才调用 `xTimerCreateTimerTask()` 创建定时器服务任务。

内核任务创建成功后才进入 port 启动路径。动态创建可能因堆不足而失败；静态创建要求回调提供有效且足够大的缓冲区。启动失败时应检查内存、回调和配置，启用 `configASSERT` 有助于发现不满足的前置条件。此 port 正常启动后不返回调用者，应用应处理启动调用意外返回的情况。

#### 设置 BASEPRI 与内核状态

本 port 的中断屏蔽操作如下：

```c
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

`BASEPRI` 属于特殊寄存器，可通过 `MSR`/`MRS` 访问。非零值建立可配置异常的优先级屏蔽阈值：优先级编码值大于等于该阈值的可配置异常不能激活。数值越小，紧急度越高；更高紧急度的中断仍可响应。`BASEPRI = 0` 表示取消这一优先级屏蔽。NMI 和 HardFault 不受 BASEPRI 屏蔽。

下图摘自 ARMv7-M Architecture Reference Manual 的 B1.4.3，说明特殊屏蔽寄存器的作用。

![BASEPRI 定义][2]

Cortex-M 的中断嵌套受抢占优先级、优先级分组和屏蔽寄存器共同约束。Reset、NMI 和 HardFault 具有固定优先级，其余可配置异常的有效优先级位位于优先级字段的高位。

![优先级位与寄存器编码值][3]

`configMAX_SYSCALL_INTERRUPT_PRIORITY` 是写入 BASEPRI 的寄存器编码值，有效优先级位必须形成非零阈值。常见配置中的 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 是未移位的逻辑值；例如实现 4 个优先级位时，逻辑值 5 对应编码值 `0x50`。

内核随后设置运行状态、初始 tick 和下一次解除阻塞的时间；启用相关配置时，还初始化运行时统计计时器和当前任务的 C 运行时线程局部存储上下文，再调用 `xPortStartScheduler()`。

#### 配置异常优先级与节拍

启用 `configASSERT` 时，`xPortStartScheduler()` 会探测已实现的优先级位，检查内核优先级及声明的优先级位数，并计算允许的优先级分组范围。实际分组值由 `vPortValidateInterruptPriority()` 在 ISR API 校验路径中检查。应用应保持与 port 要求一致的分组：有效优先级位用于抢占优先级。

| 异常或中断 | 配置与限制 |
| --- | --- |
| PendSV、SysTick | 由 `configKERNEL_INTERRUPT_PRIORITY` 的寄存器编码值配置为最低紧急度 |
| 调用内核 API 的外部中断 | 仅使用允许在 ISR 中调用的 `...FromISR()` 接口，紧急度不得高于 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 阈值 |
| 紧急度高于阈值的外部中断 | 不调用 FreeRTOS API，其与任务或内核共享数据时需要独立的同步设计 |
| SVC | 首次启动时必须不受 BASEPRI 阈值屏蔽；此 port 不设置其优先级，保留复位默认值 0 可满足这一要求 |

PendSV 和 SysTick 的优先级写入 SHPR3 对应字段：

```c
portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;
```

随后调用 `vPortSetupTimerInterrupt()` 初始化节拍中断。该函数的默认实现使用 SysTick，也可由应用覆盖为自定义节拍源。port 将临界区嵌套计数归零，通过 `prvEnableVFP()` 使能 CP10/CP11，并设置 FPCCR 的 ASPEN、LSPEN 位以启用浮点上下文自动保存与延迟保存，再进入 `prvStartFirstTask()`。

#### prvStartFirstTask

```asm
PRESERVE8
```

`PRESERVE8` 声明此汇编代码保持 8 B 栈对齐约定，用于汇编器及链接器的约束检查。实际对齐由进入时的栈指针和代码中的栈操作保证。

```asm
ldr r0, =0xE000ED08
ldr r0, [r0]
ldr r0, [r0]
msr msp, r0
```

VTOR 保存当前向量表的基地址，向量表第一个 32 位字保存初始 MSP 值。代码据此将 MSP 重设到系统栈起点，原有内存内容保持不变；此前主程序的调用链不再用于正常返回。当前向量表必须提供有效、正确对齐的初始 MSP，系统栈空间也应足够容纳异常处理。

下图摘自 ARMv7-M Architecture Reference Manual 的 B3.2.5，说明 VTOR 的定义。

![VTOR 寄存器][5]

系统栈一般在启动文件或链接脚本中定义，例如 Keil 工程启动文件中的 `Stack_Size`。本 port 启动后的栈用途如下：

| 场景 | 使用的栈 |
| --- | --- |
| 任务在 Thread mode 执行 | PSP 指向各任务的私有栈，切换时由 TCB 保存恢复位置 |
| 异常处理函数在 Handler mode 执行 | 使用 MSP 系统栈，包括处理函数局部变量和异常嵌套所需空间 |
| 从任务进入异常 | 硬件自动栈帧保存在被打断任务的 PSP 栈中，随后处理函数使用 MSP |
| 从 Handler mode 进入嵌套异常 | 硬件自动栈帧保存在 MSP 栈中 |
| 首次启动的 SVC | 进入前的主程序使用 MSP，硬件自动栈帧保存在 MSP 栈中 |

```asm
mov r0, #0
msr control, r0
```

写入 `CONTROL = 0` 使 Thread mode 处于特权状态、选择 MSP，并清除 FPCA 浮点上下文活动标记，为首次 SVC 使用基本异常帧准备状态。之后通过 EXC_RETURN 切换到 PSP。任务运行期间如使用浮点指令，后续异常的扩展帧与浮点保存按 FPU 状态及 EXC_RETURN 处理。

```asm
cpsie i
cpsie f
dsb
isb
```

`cpsie i` 和 `cpsie f` 分别清除 PRIMASK 和 FAULTMASK。`DSB` 确保此前的显式内存访问完成后再继续执行，`ISB` 使后续指令在同步后的执行上下文中重新取指。

此时 BASEPRI 仍保持 `configMAX_SYSCALL_INTERRUPT_PRIORITY`，因此 SysTick 和 PendSV 仍受屏蔽，紧急度高于阈值的中断以及满足前述优先级条件的 SVC 可以响应。

```asm
svc 0
nop
nop
```

`SVC` 指令同步触发异常，进入 `vPortSVCHandler()`。此非 MPU port 通过 SVC 完成首次任务恢复，后续常规任务切换使用 PendSV。

#### vPortSVCHandler

首次进入 SVC 时，硬件将主程序的 R0-R3、R12、LR、PC 和 xPSR 保存到 MSP 栈上的基本异常帧。接下来，处理函数恢复创建任务时准备好的上下文。

```asm
ldr r3, =pxCurrentTCB
ldr r1, [r3]
ldr r0, [r1]
```

`pxCurrentTCB` 指向首次要运行的任务，TCB 的第一个成员 `pxTopOfStack` 保存其软件恢复块的地址。创建任务时，内核已将它指向最高优先级就绪任务之一；SVC 处理函数直接使用这一指针。

```asm
ldmia r0!, {r4-r11, r14}
```

恢复 R4-R11 和 R14，共 9 个 32 位字。R14 恢复为创建时写入的 `0xFFFFFFFD`，表示异常返回到 Thread mode、使用 PSP、恢复基本异常帧。任务自身的 LR 则位于硬件异常帧中，在后续异常返回时恢复。

下图摘自 ARMv7-M Architecture Reference Manual 的 B1.5.8；图中无浮点扩展帧的返回方式适用于这里构造的初始基本帧。

![EXC_RETURN 与异常返回][6]

```asm
msr psp, r0
isb
```

`LDMIA` 的地址回写已使 R0 向高地址移动 36 B，PSP 因而指向任务基本异常帧的 R0 槽位。以字节地址表示，这时 PSP 等于 TCB 保存的栈顶地址加 36 B。

```asm
mov r0, #0
msr basepri, r0
```

BASEPRI 清零，取消该优先级阈值屏蔽。具体异常能否激活，仍由其使能状态、当前执行优先级及其他架构条件共同决定。

```asm
bx r14
```

处理器识别 EXC_RETURN，从 PSP 指向的基本帧恢复 R0-R3、R12、LR、PC 和 xPSR，进入任务入口。任务在特权 Thread mode 中使用 PSP 运行；MSP 保留给 Handler mode，首次 SVC 进入时保存的主程序帧留在系统栈中。

### 首次启动的寄存器状态

下表描述初始基本帧路径，不包含更高紧急度异常插入时的瞬时状态。`栈顶` 表示首次任务的 `pxTopOfStack` 字节地址。

| 阶段 | MSP | PSP | CONTROL | BASEPRI | EXC_RETURN |
| --- | --- | --- | --- | --- | --- |
| `prvStartFirstTask()` 触发 SVC 前 | 向量表首字给出的初始值 | 尚未装载任务帧 | 0：特权、MSP、FPCA 清零 | 系统调用屏蔽阈值 | 尚未用于首次任务恢复 |
| SVC 刚进入 | 指向主程序基本帧，可能含对齐填充 | 尚未装载任务帧 | Handler mode 使用 MSP | 系统调用屏蔽阈值 | 进入 SVC 时硬件生成的返回值 |
| SVC 软件恢复完成 | 保留主程序帧 | 栈顶 + 36 B，指向任务基本帧 | Handler mode 使用 MSP | 0 | R14 已装入 `0xFFFFFFFD` |
| 异常返回任务入口 | 保留主程序帧 | 栈顶 + 68 B，基本帧已弹出 | 2：特权、PSP、FPCA 清零 | 0 | 已用于完成异常返回 |

## vTaskEndScheduler

### 功能

请求结束调度器，实际支持取决于 port。本文的 `portable/RVDS/ARM_CM4F` 未实现结束调度器并恢复启动前执行环境的流程。

`vTaskEndScheduler()` 设置中断优先级屏蔽、将调度器运行标志清零，然后调用 `vPortEndScheduler()`。该 port 的后者只有用于提示不支持操作的断言，没有停止节拍计数器、恢复主程序上下文或自动清理所有任务资源的实现。需要停止业务时，应由应用协调任务和外设的退出，不能依赖此接口返回启动前的主程序。

### 原型

```c
void vTaskEndScheduler( void );
```

## 源码依据

- [tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：`vTaskStartScheduler()`、`vTaskEndScheduler()` 和首次任务的 TCB 选择。
- [include/task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：启动与结束调度器的接口约定。
- [portable/RVDS/ARM_CM4F/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/port.c)：优先级校验、首次 SVC、栈恢复、浮点设置和结束调度器的 port 实现。
- [portable/RVDS/ARM_CM4F/portmacro.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/portmacro.h)：BASEPRI 屏蔽操作。

 [2]: ./images/basepri.jpg
 [3]: ./images/priority_value.jpg
 [5]: ./images/vtor_register.jpg
 [6]: ./images/exc_return_definition.jpg
