# 如何检测栈溢出

## 问题背景

栈按后进先出的方式使用，可保存函数调用、局部变量和寄存器上下文等。任务栈通常由 RTOS 或应用预留，编译器生成的代码在调用过程中使用和回收栈帧，并不是每次调用都重新申请整块任务栈。

当任务或 ISR 的栈使用超出预留边界时，可能破坏邻接数据、控制块或返回地址。在常见的 Cortex-M PSP 任务模型中，任务使用 PSP，Handler 模式使用 MSP；从任务进入异常时，自动入栈帧仍占用被打断任务的 PSP 栈，因此任务栈预算也要计入异常帧、对齐和可能的浮点上下文，MSP 则要考虑嵌套中断。

常见的错误原因如下：

- 栈空间预留不足，或未计入异常嵌套和上下文保存开销。

- 函数内使用了较大的自动变量，比如定义一个大数组。

- 递归调用过深。

## 软件检测

### FreeRTOS

`configCHECK_FOR_STACK_OVERFLOW` 控制任务栈的软件检查，部分 port 另有 ISR 栈检查，不能把它理解为所有 port 统一支持的三档功能。`vTaskSwitchContext()` 在切出当前任务、选择下一任务前调用 `taskCHECK_FOR_STACK_OVERFLOW()`；检查的是当时保存的状态，不是每次栈访问。

| 配置 | 任务栈检查 | ISR 栈检查 |
| --- | --- | --- |
| `0` | 不启用此软件检查 | 不由此配置保证 |
| `1` | 比较保存的 SP 与栈边界 | 不由通用任务栈宏提供 |
| `> 1` | 检查栈边界填充值；本文向低地址增长的分支检查低地址端 16 B | 是否额外支持由 port 决定 |
| `> 2` | 仍满足 `> 1`，任务栈异常仍调用 `vApplicationStackOverflowHook()` | 仅在提供相应实现、配置及调用路径的 port 上附加检查 |

以下保留向低地址增长栈的实现片段，不是完整工程或统一 port 接口；`portSTACK_LIMIT_PADDING` 等 port 定义需在实际实现中核实。分支语义对照 [V10.5.1 的 stack_macros.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/stack_macros.h)，调用时机见 [tasks.c 的 vTaskSwitchContext()](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)。片段不能作为上述 tag 的逐字源码引用。

1. `configCHECK_FOR_STACK_OVERFLOW = 1`

     在检查点比较已保存的栈指针与栈边界。下行栈在低地址端检查；上行栈使用另一分支。若任务先越界、再恢复 SP，检查可能漏掉历史越界；越界也可能在检查前破坏 TCB 或其他对象。

     ```c
    #if ( ( configCHECK_FOR_STACK_OVERFLOW == 1 ) && ( portSTACK_GROWTH < 0 ) )

        /* Only the current stack state is to be checked. */
        #define taskCHECK_FOR_STACK_OVERFLOW()                                                      \
        do {                                                                                        \
            /* Is the currently saved stack pointer within the stack limit? */                      \
            if( pxCurrentTCB->pxTopOfStack <= pxCurrentTCB->pxStack + portSTACK_LIMIT_PADDING )     \
            {                                                                                       \
                char * pcOverflowTaskName = pxCurrentTCB->pcTaskName;                               \
                vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pcOverflowTaskName ); \
            }                                                                                       \
        } while( 0 )

    #endif /* configCHECK_FOR_STACK_OVERFLOW == 1 */
    ```

2. `configCHECK_FOR_STACK_OVERFLOW > 1`

     任务创建时按配置填充栈；下列分支检查低地址端四个 `uint32_t`，即 16 B 是否仍为 `0xA5`。V10.5.1 的上行栈分支在高地址端比较 20 B，不能把 16 B 泛化到所有栈方向与 port。

     这是边界哨兵检查，不是扫描整个已用栈，也不保证同时执行方式一的 SP 比较。跨过哨兵后才写入、只改变 SP 而不写哨兵、写入值恰好相同或被恢复，都可能漏检；hook 被调用时破坏可能已经发生。

     ```c
    #if ( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) && ( portSTACK_GROWTH < 0 ) )

        #define taskCHECK_FOR_STACK_OVERFLOW()                                                      \
        do {                                                                                        \
            const uint32_t * const pulStack = ( uint32_t * ) pxCurrentTCB->pxStack;                 \
            const uint32_t ulCheckValue = ( uint32_t ) 0xa5a5a5a5U;                                 \
                                                                                                    \
            if( ( pulStack[ 0 ] != ulCheckValue ) ||                                                \
                ( pulStack[ 1 ] != ulCheckValue ) ||                                                \
                ( pulStack[ 2 ] != ulCheckValue ) ||                                                \
                ( pulStack[ 3 ] != ulCheckValue ) )                                                 \
            {                                                                                       \
                char * pcOverflowTaskName = pxCurrentTCB->pcTaskName;                               \
                vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pcOverflowTaskName ); \
            }                                                                                       \
        } while( 0 )

    #endif /* #if( configCHECK_FOR_STACK_OVERFLOW > 1 ) */
    ```

3. `configCHECK_FOR_STACK_OVERFLOW > 2` 的 port 扩展

     任务栈仍走 `> 1` 分支并调用任务 hook。ISR 栈是否额外检查，要核对实际 port、栈的分配与填充、检查调用点和 `configASSERT` 的有效定义；不能仅把配置值设为 `3` 就认定 ISR 已被保护。例如 [V10.5.1 portable/RVDS/ARM_CM4F/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/port.c) 不提供此类 `> 2` ISR 哨兵检查。

    下列片段展示 ISR 哨兵经 `configASSERT` 报告的写法，与任务 hook 是两条路径。[V10.5.1 portable/MPLAB/PIC32MX/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/MPLAB/PIC32MX/port.c) 在 `> 2` 时初始化 `xISRStack` 为 `0xEE`，并在 tick handler 中调用 `portCHECK_ISR_STACK()` 比较底部 20 B；它不是在每次 ISR 入栈前检查。该栈用于会引发上下文切换的 ISR，不能泛化到芯片上的任意异常栈。

    [V10.5.1 portable/GCC/RISC-V/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/GCC/RISC-V/port.c) 中，类似宏的定义条件还包括 `defined(configISR_STACK_SIZE_WORDS)`；该文件对此分支带有未测试甚至未编译的 `#warning`，且文件内只有宏定义、没有调用点，不能据此认定 ISR 检查已执行。

     ```c
    #if( configCHECK_FOR_STACK_OVERFLOW > 2 )

        /* Don't use 0xa5 as the stack fill bytes as that is used by the kernel for
        the task stacks, and so will legitimately appear in many positions within
        the ISR stack. */
        #define portISR_STACK_FILL_BYTE 0xee

        static const uint8_t ucExpectedStackBytes[] = {
                                        portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,     \
                                        portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,     \
                                        portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,     \
                                        portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,     \
                                        portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE };   \

        #define portCHECK_ISR_STACK() configASSERT( ( memcmp( ( void * ) xISRStack, ( void * ) ucExpectedStackBytes, sizeof( ucExpectedStackBytes ) ) == 0 ) )
    #else
        /* Define the function away. */
        #define portCHECK_ISR_STACK()
    #endif /* configCHECK_FOR_STACK_OVERFLOW > 2 */
    ```

### RTX

以 CMSIS_5 tag `5.9.0` 中的 RTX 为例，`OS_STACK_CHECK = 1` 用于启用线程切换时的栈检查，默认值为 `0`，且要求使用支持该功能的 RTX 源码构建变体。以 GCC Armv7-M 路径为例，实际检查代码受 `RTX_STACK_CHECK` 编译条件控制；`osRtxThreadSwitch()` 设置待切换线程，检查发生在异常处理的上下文保存路径，而不是该 C 函数直接检查栈。

`osRtxThreadStackCheck(const os_thread_t *thread)` 同时检查保存的 `thread->sp <= thread->stack_mem` 和栈底 magic word 是否被改写；任一条件成立均报告失败。magic word 在创建线程时初始化，不以开启整栈 watermark 为前提。失败后，异常路径用错误码 `osRtxErrorStackOverflow` 调用 `osRtxKernelErrorNotify()`，后者转调应用可覆盖的 `osRtxErrorNotify()`。检查仍有时间窗口和跨哨兵漏检风险，错误回调不意味着可以安全返回到损坏的线程。

以下保留片段表达 SP 与 magic word 的组合判断，但其无参弱函数形式、直接调用回调以及 `osRtxErrorStackUnderflow` 名称不对应上述 tag 的实现，不应直接替代该版本源码。固定来源：[RTX_Config.h](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/RTOS2/RTX/Config/RTX_Config.h)、[rtx_thread.c](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/RTOS2/RTX/Source/rtx_thread.c)、[irq_armv7m.S](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/RTOS2/RTX/Source/GCC/irq_armv7m.S)、[rtx_kernel.c](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/RTOS2/RTX/Source/rtx_kernel.c)、[RTX_Config.c](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/RTOS2/RTX/Config/RTX_Config.c)。

```c
__WEAK void osRtxThreadStackCheck (void) {
  os_thread_t *thread;

  thread = osRtxThreadGetRunning();
  if (thread != NULL) {
    if ((thread->sp <= (uint32_t)thread->stack_mem) ||
        (*((uint32_t *)thread->stack_mem) != osRtxStackMagicWord)) {
      (void)osRtxErrorNotify(osRtxErrorStackUnderflow, thread);
    }
  }
}
```

## 硬件检测

### Watchpoint 与 DWT

Watchpoint 监控的是匹配条件下的处理器数据访问，不是 SP 的连续范围检查。以实现相应调试功能的 Cortex-M4/M4F 为例，可用 DWT 比较器监控栈低地址端预留哨兵区的写访问；不能仅凭“Armv7 之后”推断任意内核或芯片都支持。

| 条件 | 含义与边界 |
| --- | --- |
| DWT 与比较器资源 | Cortex-M4 的完整 DWT 可有 4 个比较器，精简实现可只有 1 个；无调试实现则无 watchpoint。应核对芯片手册和 `DWT_CTRL.NUMCOMP`，不能假定 `HAL_DWT_3` 对应的资源存在或空闲。 |
| 地址、长度与访问类型 | 按具体比较器配置地址匹配、掩码及写访问功能，满足范围对齐要求。片段的 `0x3` 是否表示 8 B 范围取决于 HAL 定义，不能仅凭注释确定。只改 SP、写到监控区之外，或一次跨过 guard 后才写入，都可能漏检。 |
| Halting debug | `DHCSR.C_DEBUGEN = 1` 时，已启用的 watchpoint 调试事件可使内核停机，由调试器处理；不能同时假设会进入应用的 DebugMonitor handler。 |
| Monitor debug | 非停机调试路径需要 `C_DEBUGEN = 0`、`DEMCR.TRCENA = 1`、`DEMCR.MON_EN = 1`，以及正确的 DWT 功能配置、调试访问许可、DebugMonitor 向量与可响应的优先级/屏蔽状态；命中不保证立即执行 handler。 |
| 资源归属与切换 | 调试器可能占用或重写比较器。若只监控当前任务，需由实际 port 在合适的上下文切换位置更新比较器并处理重配窗口，且另行覆盖 ISR/MSP 栈；一个函数定义本身不会自动接入调度器。 |

Watchpoint 用于定位触碰边界的访问，不是内存访问权限保护；触发写访问可能已经完成，不能据此保证邻接数据未被改写，也不应把它当作 DMA 等所有总线访问的检测器。

以下是依赖平台扩展的伪代码片段，不是 FreeRTOS 官方 API 或可直接移植的完整示例。`uxTaskGetBottomOfStack(NULL)`、`hal_dwt_request_watchpoint()`、`HAL_DWT_3` 和 `WDE_DATA_WO` 的实现未给出；实际集成需明确任务句柄、栈方向与边界、预留监控区及 HAL 编码。

片段中 DEMCR 的条件 `!(DEMCR & (MON_EN | TRCENA))` 只判断两位是否全为零：全为零时会补齐两位，已有任意一位置位时则跳过赋值，缺失的另一位仍为零。要依赖 monitor 路径，初始化必须保证两位都已置位，并保留 DEMCR 其他控制位；该片段本身不保证补齐。外层 `C_DEBUGEN` 判断也使停机调试时跳过 monitor 初始化，不能把连接调试器和脱机运行视为同一路径。

固定依据：Arm Cortex-M4 TRM，文档 `100166/0001`、r0p1 的 [Debug configuration（第 8 章）](https://support.arm.com/documentation/100166/0001/Debug/Debug-configuration) 与 [DWT functional description（第 9 章）](https://support.arm.com/documentation/100166/0001/Data-Watchpoint-and-Trace-Unit/DWT-functional-description)；寄存器位定义见 [CMSIS_5 5.9.0 core_cm4.h](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/Core/Include/core_cm4.h)。

```c
void vPortCurrentTaskStackOverflowCheck(void)
{
    uint32_t stack_start_address;

    stack_start_address = uxTaskGetBottomOfStack(NULL);

    /* enable debug monitor mode    */
    if (!(CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)) {
        if (!(CoreDebug->DEMCR & (CoreDebug_DEMCR_MON_EN_Msk | CoreDebug_DEMCR_TRCENA_Msk))) {
            CoreDebug->DEMCR |= (CoreDebug_DEMCR_MON_EN_Msk | CoreDebug_DEMCR_TRCENA_Msk) ;
        }
    }

    /* check the last 2words */
    hal_dwt_request_watchpoint(HAL_DWT_3, stack_start_address, 0x3, WDE_DATA_WO);
}
```

### Armv8-M 栈限与 STKOF

栈限功能属于相应的 Armv8-M 配置，不能推广到所有 Armv8 内核。以下以 Armv8-M Mainline 的 Cortex-M33 为例；Cortex-M4/M4F 没有这些栈限寄存器。

| 架构与安全配置 | 栈限寄存器及适用条件 |
| --- | --- |
| Cortex-M33，未实现 Security Extension | `MSPLIM` 对应 MSP，`PSPLIM` 对应 PSP，两套栈各有下限。 |
| Cortex-M33，实现 Security Extension | Secure 与 Non-secure 状态各有 `MSPLIM`/`PSPLIM`，共四个栈限寄存器；需要分别初始化，不会因配置一侧而自动保护另一侧。Secure 软件可通过相应 `_NS` 接口访问 Non-secure 栈限。 |
| Armv8-M Baseline，如带 Security Extension 的 Cortex-M23 | 不能照搬 M33 的四套栈限与 UsageFault 路径：Secure 栈支持相应栈限，Non-secure 栈限未实现，读零、写忽略；Baseline 也不提供独立的 UsageFault 异常。未实现 Security Extension 时不能假定存在 Secure 栈限。 |

在 M33 上，`MSPLIM`/`PSPLIM` 的有效下限按 8 B 对齐，复位值为 `0`，仅有寄存器并不会自动获得应用栈的边界。RTOS 需在任务创建、首次启动及任务切换时配套设置/恢复 PSP 下限；MSP 及不同安全状态的栈应独立配置并留出异常处理空间。

处理器对规范规定的 SP 更新与入栈操作检查下限，越过下限时可报告栈溢出。在 M33 的 UFSR 中，`STKOF` 是 bit 4，对应整个 CFSR 的 bit 20。它是故障状态位，不是检测使能位；该机制不是整个栈内存的访问权限检查，也不能捕获所有通过普通地址寄存器产生的越界写。

要进入独立的 UsageFault handler，需要在相应安全状态的 `SCB.SHCSR.USGFAULTENA` 使能该异常、连接正确的向量，并使其具有可响应的执行优先级。未启用或发生时不能抢占当前执行优先级等情况会升级为 HardFault；在故障处理中再次出错还可能进入 lockup。故障状态、异常帧与 SP 不应未经检查就用于恢复执行。

固定依据：Arm Cortex-M33 Devices Generic User Guide，文档 `100235/0004`、r0p4，第 2 章 [Core registers / Stack limit registers](https://support.arm.com/documentation/100235/0004/the-cortex-m33-processor/programmer-s-model/core-registers)、[Fault escalation to HardFault](https://support.arm.com/documentation/100235/0004/the-cortex-m33-processor/fault-handling/fault-escalation-to-hardfault)，第 4 章 [CFSR / UFSR](https://support.arm.com/documentation/100235/0004/the-cortex-m33-peripherals/system-control-block/configurable-fault-status-register) 和 [SHCSR](https://support.arm.com/documentation/100235/0004/the-cortex-m33-peripherals/system-control-block/system-handler-control-and-state-register)。Baseline 的 Non-secure 限制也可对照 [CMSIS_5 5.9.0 cmsis_gcc.h 的 __get/__set_MSPLIM、PSPLIM 条件](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/Core/Include/cmsis_gcc.h)。

集成示例可查看 [V10.5.1 GCC ARM_CM33_NTZ/non_secure/port.c 的栈初始化](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/GCC/ARM_CM33_NTZ/non_secure/port.c) 和 [portasm.c 的首次恢复与 PendSV 保存/恢复 PSPLIM](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c)。这些代码展示特定 port 的任务 PSP 下限管理，不是完整的 MSP、安全状态、异常 handler 配置或可直接运行的板级例程。

### MPU

MPU（Memory Protection Unit）按实际访问地址及权限检查内存。只有实现并启用相应 MPU、在下行栈低地址侧配置 guard（禁止相关访问的保护区或未映射空洞），且任务/异常使用的权限确实受限制，才可能在触碰边界时阻止访问并报告 MemManage fault；未使能该异常或无法响应时可能升级 HardFault。仅在芯片上“有 MPU”或只开启 MPU，并不自动产生栈保护。

| 约束 | 设计时需明确的边界 |
| --- | --- |
| 粒度与区域数 | Cortex-M4 的 MPU 区域使用 2 的幂大小并要求相应基址对齐；M33 的 base/limit 以 32 B 为粒度。需按实际 MPU 的最小尺寸、区域数与链接布局预留空间，不能随意保护单个字节。 |
| 权限与特权执行 | 只禁止非特权访问的区域不阻止特权任务或 ISR。未映射空洞若允许 `PRIVDEFENA` 特权背景映射，也可能被特权访问绕过；这不意味着显式拒绝特权访问的有效区域会被背景映射覆盖。特权软件还可重配或关闭 MPU，`HFNMIENA` 决定 HardFault/NMI 等状态下是否继续应用保护。 |
| 区域匹配规则 | M4 的重叠区域按优先级选择，M33 的重叠匹配会故障；不能把 M4 的覆盖技巧直接套到 M33。任务切换必须配套恢复区域及权限，不把短暂重配窗口当成受保护状态。 |
| 跨越 guard | 若 SP 一次越过有限 guard，并直接在另一段仍允许访问的内存中写入，MPU 可以不报错；要降低此风险需结合不映射相邻区域、限制最大栈帧或编译器栈探测等设计，不能宣称有限 guard 覆盖所有溢出。 |

固定依据：上述 Cortex-M4 TRM 的 [MPU functional description](https://support.arm.com/documentation/100166/0001/Memory-Protection-Unit/MPU-functional-description)，以及 Cortex-M33 用户指南第 4 章 [Memory Protection Unit](https://support.arm.com/documentation/100235/0004/the-cortex-m33-peripherals/security-attribution-and-memory-protection/memory-protection-unit)、[MPU_RBAR](https://support.arm.com/documentation/100235/0004/the-cortex-m33-peripherals/security-attribution-and-memory-protection/mpu-region-base-address-register)、[MPU_CTRL](https://support.arm.com/documentation/100235/0004/the-cortex-m33-peripherals/security-attribution-and-memory-protection/mpu-control-register)。寄存器粒度可对照 [CMSIS_5 5.9.0 core_cm33.h](https://github.com/ARM-software/CMSIS_5/blob/5.9.0/CMSIS/Core/Include/core_cm33.h)。
