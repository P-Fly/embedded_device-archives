# 中断优先级

以下按 `portable/RVDS/ARM_CM4F` 的 Cortex-M4F 非 MPU port 说明外部中断、BASEPRI、PendSV 和 SysTick 的约束；SVC 另列 `portable/RVDS/ARM_CM4_MPU` 的具体实现。是否采用 MPU port 由项目选择的移植层和配置决定。

## 外部中断优先级

中断优先级数值越小，紧急度越高。NVIC 的优先级字段宽度为 8 位，芯片实现的有效位位于高位。配置时需要区分逻辑优先级和寄存器编码：

| 配置或接口 | 使用的数值 |
| --- | --- |
| CMSIS `NVIC_SetPriority()` | 未移位的逻辑优先级，由 CMSIS 按有效位数移位后写入寄存器 |
| `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` | 常见应用配置中定义的未移位逻辑阈值 |
| `configMAX_SYSCALL_INTERRUPT_PRIORITY` | port 写入 BASEPRI 的寄存器编码阈值，有效位必须形成非零值 |
| `configLIBRARY_LOWEST_INTERRUPT_PRIORITY` | 常见应用配置中定义的最低紧急度逻辑值 |
| `configKERNEL_INTERRUPT_PRIORITY` | port 配置 PendSV、SysTick 时使用的寄存器编码值 |

例如实现 4 个优先级位、所有有效位用于抢占优先级时，逻辑范围为 0 至 15。逻辑阈值 5 左移 4 位得到编码值 `0x50`，最低紧急度 15 对应 `0xF0`。

| 逻辑优先级 | BASEPRI 为 `0x50` 时 | FreeRTOS API 使用范围 |
| --- | --- | --- |
| 0 至 4 | 不受该阈值屏蔽 | 不调用 FreeRTOS API，采用独立的同步设计 |
| 5 至 15 | 受该阈值屏蔽 | 可以使用该 API 明确支持的 ISR 调用方式，并满足其参数与对象限制 |

阈值同级包含在可调用内核 ISR API 的范围内。中断处理函数使用文档允许的 ISR 接口，常见名称以 `FromISR` 结尾；还应检查调用者的优先级、对象类型、参数及调度器状态。优先级合规与接口合规需要同时满足。

BASEPRI 不改变中断的 NVIC 使能位，而是在非零时屏蔽达到阈值及更低紧急度的可配置异常；清零表示取消这一阈值屏蔽。更高紧急度的中断仍可响应，NMI 和 HardFault 不受 BASEPRI 屏蔽。

## 优先级分组与检查

NVIC 的分组可以将有效优先级位分为抢占优先级和子优先级。同一抢占优先级的异常不会相互抢占，子优先级只参与多个待处理异常的先后选择。此 port 要求所有已实现的优先级位用于抢占优先级。

使用 CMSIS 时，`NVIC_SetPriorityGrouping(0)` 可满足该分组要求。若芯片厂商库另有分组约定，应统一整个工程的设置，避免初始化代码随后改写分组。`NVIC_EncodePriority()` 根据传入的分组值组合抢占优先级和子优先级，应用应使该参数与实际分组一致；其结果仍是传给 `NVIC_SetPriority()` 的逻辑值。

启用 `configASSERT` 后，校验分为两个阶段：

| 路径 | 校验内容 |
| --- | --- |
| `xPortStartScheduler()` | 探测已实现的优先级位，检查内核优先级及声明的位数，计算允许的分组范围 |
| 带中断优先级校验的 ISR API 路径 | `vPortValidateInterruptPriority()` 检查当前外部中断是否达到系统调用阈值，并检查实际优先级分组 |

`__NVIC_PRIO_BITS` 和配置中使用的有效位数应与芯片实现一致。会调用内核 ISR API 的外部中断应显式设置合规优先级；Cortex-M 的外部中断优先级复位值为 0，高于非零的系统调用阈值。

## ISR 中的内核调用

选择接口时，需要同时满足以下条件：

- 运行上下文和优先级合规，使用该 API 文档明确支持的 ISR 入口。`portYIELD_FROM_ISR()` 等 port 辅助宏也按其接口约定使用。
- 对象已成功创建，句柄仍有效；满足该接口对缓冲区、计数和对象类型的要求。互斥量的所有权和优先级继承属于任务上下文，不在 ISR 中获取或释放互斥量。
- 使用 `pxHigherPriorityTaskWoken` 时，在一次 ISR 的首次相关调用前初始化为 `pdFALSE`。同一 ISR 连续调用多个接口时保留累计结果，退出前据此调用 `portYIELD_FROM_ISR()` 请求切换。
- 调度器启动前可创建任务和内核对象，运行期 ISR 调用还须满足各接口的调度状态前提。挂起调度器只暂停部分调度处理，ISR 仍可能执行；内核会按相关接口路径暂存唤醒或切换请求。

优先级高于阈值的 ISR 与任务共享数据时，需要独立设计同步方式。内核临界区通过 BASEPRI 保护的范围不包含这些 ISR。

## SVC 优先级

### 非 MPU Cortex-M4F port

`portable/RVDS/ARM_CM4F` 用 SVC 恢复首个任务，后续常规切换使用 PendSV。该 port 不写入 SVC 优先级；首次启动时 BASEPRI 仍保持系统调用阈值，因此 SVC 必须具有高于该阈值的紧急度。保留复位默认优先级 0 可以满足这一要求；若应用修改了 SVC 优先级，应在启动前检查。

### Cortex-M4 MPU port

`portable/RVDS/ARM_CM4_MPU/port.c` 定义了以下宏，并在处理启动调度器的 SVC 时，将其写入 SHPR2 的 SVC 优先级字段：

```c
    #define portNVIC_SVC_PRI    ( ( ( uint32_t ) configMAX_SYSCALL_INTERRUPT_PRIORITY - 1UL ) << 24UL )
```

宏先对已编码的 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 减 1，再左移 24 位放入 SHPR2 对应字段。以 4 个优先级位、阈值 `0x50` 为例，减 1 得到 `0x4F`，写入时未实现的低 4 位被忽略，有效优先级字段为 `0x40`，对应逻辑优先级 4。

此 MPU port 的 SVC 还参与特权操作，具体服务及权限检查由相应 wrapper 和配置决定。该宏及其启动处理路径限定于这里列出的 MPU port。

## PendSV 优先级

在此 Cortex-M4F port 中，PendSV 由 `configKERNEL_INTERRUPT_PRIORITY` 配置为最低紧急度。这样，从 ISR 请求的调度会推迟到不再有外部 ISR 正在执行时，避免在仍需返回 ISR 的情况下恢复另一个任务的上下文。

PendSV pending 是单个位，多个请求可以合并，处理函数按执行时的就绪状态选择任务。如果处理期间又收到请求，退出后仍可能再次进入；一次请求也不保证切换到另一个任务。

## SysTick 优先级

`portable/RVDS/ARM_CM4F` 同样将 SysTick 配置为最低紧急度。`xPortSysTickHandler()` 依赖这一入口条件：调用 `vPortRaiseBASEPRI()` 保护节拍处理，随后将 BASEPRI 清零，不保存和恢复旧阈值。优先级配置需要保持这一实现假设。

SysTick 处理到期任务后，仅在内核返回需要调度时置位 PendSV。任务真正开始执行还取决于中断屏蔽、其他异常的执行时间、PendSV 的响应及任务优先级。任务响应时间分析应包括临界区长度、高紧急度 ISR 的执行与到达频率，以及任务本身的阻塞和运行开销。

若应用替换默认节拍源或采用其他 port，应按实际节拍处理函数的屏蔽和嵌套设计确定优先级，并保留对临界区和任务响应时间的验证。

## 源码依据

- [portable/RVDS/ARM_CM4F/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/port.c)：异常优先级设置、SysTick 屏蔽假设及 `vPortValidateInterruptPriority()`。
- [portable/RVDS/ARM_CM4F/portmacro.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/portmacro.h)：BASEPRI 保存与恢复、ISR 切换请求宏。
- [portable/RVDS/ARM_CM4_MPU/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4_MPU/port.c)：`portNVIC_SVC_PRI` 及启动 SVC 的优先级设置。
- [include/task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：任务及 ISR 接口、调度器状态的使用约定。
- [CMSIS 5.9.0 core_cm4.h](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/Core/Include/core_cm4.h)：`NVIC_SetPriority()`、`NVIC_SetPriorityGrouping()` 和 `NVIC_EncodePriority()`。
