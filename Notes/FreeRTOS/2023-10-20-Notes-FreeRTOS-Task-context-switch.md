# 任务切换

以下汇编按 `portable/RVDS/ARM_CM4F` 的非 MPU Cortex-M4F port 分析，使用 Arm Compiler 5 的汇编语法。任务在特权 Thread mode 中使用 PSP，异常处理函数在 Handler mode 中使用 MSP。片段按步骤展示核心寄存器的处理，浮点上下文的条件分支在相邻文字中说明。

任务切换的请求路径如下图，具体触发条件见下表。

![任务切换请求路径概览][1]

| 路径 | 请求条件 |
| --- | --- |
| 任务调用 `taskYIELD()` | 通过 `portYIELD()` 请求调度，在抢占和协作式调度模式下均有效 |
| 内核使用 `taskYIELD_IF_USING_PREEMPTION()` | `configUSE_PREEMPTION == 1` 时请求调度；这是内核内部宏，应用使用公开任务 API |
| ISR 使用 `portYIELD_FROM_ISR()` / `portEND_SWITCHING_ISR()` | 参数为非 `pdFALSE` 时请求调度；通常由 `...FromISR()` API 的唤醒结果决定 |
| `xPortSysTickHandler()` | 仅当 `xTaskIncrementTick()` 返回非 `pdFALSE` 时请求调度 |

节拍处理会将延时到期的任务移入就绪链表。在抢占模式下，唤醒的任务优先级高于当前任务，或内核已有 `xYieldPending` 请求时，需要调度；同时启用 `configUSE_TIME_SLICING` 且当前优先级有多个就绪任务时，也需要调度以实现时间片轮转。

协作式调度中，更高优先级任务就绪本身不会抢占当前任务；任务主动让出、阻塞或 ISR 显式请求切换时，内核重新选择可运行任务。调度器挂起时，节拍先累计到 `xPendedTicks`，在最外层恢复调度器时再处理。

## 触发任务切换

这些路径最终通过写入 ICSR 的 `PENDSVSET` 位，将 PendSV 置为 pending：

```c
portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
```

`portYIELD()` 还包含 DSB、ISB 屏障。pending 位表示一次待处理请求，多个请求可以合并，既不记录请求次数，也不指定下一个任务。

此 port 将 PendSV 配置为最低紧急度。其实际执行受 BASEPRI、PRIMASK 和当前异常优先级等条件约束；从 ISR 请求时，通常在异常返回路径中通过尾链进入 PendSV。每次调度按当时的就绪状态选择任务，也可能继续运行原任务。

## PendSV Exception

### 原型

```c
void xPortPendSVHandler( void );
```

### 流程

#### 异常入口与栈

任务从 Thread mode 进入异常时，硬件在其 PSP 栈上保存 R0-R3、R12、LR、PC 和 xPSR，构成 8 个 32 位字的基本异常帧。Handler mode 随后使用 MSP 执行；若 PendSV 通过尾链进入，则沿用已经保存在任务栈上的帧。

处理函数中的 R14 此时保存 EXC_RETURN，描述异常返回的模式、栈及帧类型；任务自身的 LR 保存在硬件帧中。下面先按基本帧说明核心寄存器的保存和恢复。

```asm
PRESERVE8
```

`PRESERVE8` 声明汇编代码保持 8 B 栈对齐约定。实际对齐由进入时的栈指针及后续栈操作保证。

#### 保存当前任务

```asm
mrs r0, psp
isb
ldr r3, =pxCurrentTCB
ldr r2, [r3]
```

对寄存器进行赋值：

1. `R0 = PSP`，取得当前任务硬件异常帧的起始地址。

2. `R3 = &pxCurrentTCB`，取得保存当前 TCB 指针的全局变量地址。

3. `R2 = pxCurrentTCB`，取得当前 TCB 实例地址，其第一个成员是保存任务栈位置的 `pxTopOfStack`。

当 EXC_RETURN 的 bit 4 为 0 时，完整 port 会先将 S16-S31 保存到任务栈，再执行下面的核心寄存器保存。此时 R0 已越过这部分高位浮点寄存器保存区。

```asm
stmdb r0!, {r4-r11, r14}
```

`STMDB` 将 R4-R11 和 EXC_RETURN 共 9 个字保存到 R0 指向的任务栈，并向低地址更新 R0。这里操作的是 PSP 所属的任务栈，处理函数的活动栈指针仍为 MSP。

```asm
str r0, [r2]
```

将保存完成后的地址写入当前 TCB 的首成员 `pxTopOfStack`。以后恢复该任务时，从这一软件保存块开始读取；任务的硬件帧和可能存在的浮点帧仍保留在更高地址处。

#### 选择下一个任务

```asm
stmdb sp!, {r0, r3}
mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
msr basepri, r0
dsb
isb
bl vTaskSwitchContext
mov r0, #0
msr basepri, r0
ldmia sp!, {r0, r3}
```

`SP` 在 Handler mode 中表示 MSP。这对压栈和出栈在系统栈上暂存 R0、R3，同时保持调用 C 函数所需的 8 B 对齐。R3 保存 `&pxCurrentTCB`，用于调用返回后读取选中的任务；临时恢复的 R0 随后会被该任务的保存地址覆盖。

写入 BASEPRI 后，达到 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 阈值及更低紧急度的中断被屏蔽，以保护内核调度数据。更高紧急度的中断仍可响应，但不调用 FreeRTOS API。这个 port 的 PendSV 以最低紧急度进入，依赖入口处已知的屏蔽状态，在 C 调用后直接清零 BASEPRI；这段操作不修改临界区嵌套计数。

`vTaskSwitchContext()` 根据调度器状态执行：

| 条件 | 行为 |
| --- | --- |
| 调度器已挂起 | 设置 `xYieldPending`，保持 `pxCurrentTCB` 不变，等待调度器恢复后再调度 |
| 调度器可运行 | 从最高有效优先级的就绪链表选择任务，更新 `pxCurrentTCB`；同优先级链表通过 `pxIndex` 轮转选择 |

正常选择分支还按配置执行切出/切入追踪、运行时间统计、栈溢出检查，以及 errno、C 运行时线程局部存储的保存和切换。最高优先级只有当前任务可运行时，选择结果仍是该任务；`taskYIELD()` 也按优先级规则选择，不保证让低优先级任务运行。

#### 恢复选中的任务

```asm
ldr r1, [r3]
ldr r0, [r1]
```

通过更新后的 `pxCurrentTCB` 取得选中任务的 TCB，再读取首成员 `pxTopOfStack`，使 R0 指向该任务的软件保存块。

```asm
ldmia r0!, {r4-r11, r14}
```

恢复 R4-R11 和该任务的 EXC_RETURN，R0 向高地址移动 36 B。当恢复的 EXC_RETURN bit 4 为 0 时，完整 port 接着恢复 S16-S31，R0 再移动 64 B；该值为 1 时，直接继续处理基本帧。

```asm
msr psp, r0
isb
```

将 PSP 设置为选中任务的硬件异常帧地址。TCB 中保存的地址暂时保持不变，下次切出时再由保存路径更新。

```asm
bx r14
```

处理器根据 EXC_RETURN 从 PSP 恢复硬件帧，回到特权 Thread mode，使用该任务的 PSP、PC 和其他寄存器继续运行。若选中的是从未运行过的任务，则恢复创建时构造的初始基本帧并进入任务入口。

## 源码依据

- [portable/RVDS/ARM_CM4F/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/port.c)：`xPortPendSVHandler()`、`xPortSysTickHandler()` 和浮点上下文分支。
- [portable/RVDS/ARM_CM4F/portmacro.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/portmacro.h)：`portYIELD()`、`portEND_SWITCHING_ISR()` 和 `portYIELD_FROM_ISR()`。
- [tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：`vTaskSwitchContext()`、`xTaskIncrementTick()`、任务选择宏和 TCB 首成员。
- [include/task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：`taskYIELD()` 及公开调度接口的约定。

 [1]: ./images/task_yield.jpg
