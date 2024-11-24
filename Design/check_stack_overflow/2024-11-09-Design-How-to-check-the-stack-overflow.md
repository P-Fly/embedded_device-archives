# 如何检测栈溢出

## 问题背景

**栈** 是一种 **后进先出** 的数据结构。一般由任务自动分配和释放，用于存放函数的参数值，局部变量，返回地址等信息。

当 **任务** 或 **ISR** 使用的栈空间超过了预留的内存时，就会发生 **栈溢出** 错误，这种错误通常会导致程序出现异常。

常见的错误原因如下：

 - ‌栈空间大小申请不足。
‌
 - 函数内使用了较大的内存区域，比如定义一个大数组。

 - 递归调用过深。

**栈溢出** 的检测方式比较简单，一般分为 **软件检测** 和 **硬件检测** 两种方式。

## 软件检测

### **FreeRTOS**

**FreeRTOS** 使用宏 **configCHECK_FOR_STACK_OVERFLOW** 来配置 **栈溢出** 的检测方式，总共有三种方式：

 1. **configCHECK_FOR_STACK_OVERFLOW = 1**

    当使用 **vTaskSwitchContext** 进行任务切换时，通过 **检测栈指针是否处于有效栈空间内**，来判断是否发生了 **栈溢出** 错误。

    此方法只能检测 **任务栈** 的溢出，且不能保证可以捕获所有 **栈溢出** 错误。

    ```
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

 2. **configCHECK_FOR_STACK_OVERFLOW = 2**

    任务首次创建时，栈空间会被一个已知值填充。当使用 **vTaskSwitchContext** 进行任务切换时，通过 **检测最后 16 个字节是否仍为填充值**，来判断是否发生了 **栈溢出** 错误。

    此方法只能检测 **任务栈** 的溢出，且不能保证可以捕获所有 **栈溢出** 错误。

    ```
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

 3. **configCHECK_FOR_STACK_OVERFLOW = 3**

    与 **方式二** 相似，但是增加了 **ISR** 的溢出检测。

    当发生 **栈溢出** 错误时，不会调用钩子函数 **vApplicationStackOverflowHook**，而是触发断言 **configASSERT**。

    此方法仅适用于特定的芯片，比如 **RISC-V** / **PIC32MX**，且不能保证可以捕获所有 **栈溢出** 错误。

    ```
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

### **RTX**

与 **FreeRTOS** 的 **方式一** 相似，当使用 **osRtxThreadSwitch** 进行任务切换时，通过 **检测栈指针是否处于有效栈空间内**，来判断是否发生了 **栈溢出** 错误。

```
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

### **WatchPoint**

**ARMv7** 架构之后的内核自带硬件 **WatchPoint** 功能，芯片一般会支持多组 **WatchPoint**。

**WatchPoint** 功能启用之后，一旦对监控区域的内存地址进行读或写操作，硬件将会自动进入 **Halt** 或者 **Exception** 状态。

因此我们可以使用该模块检测是否发生了 **栈溢出** 错误。

```
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

### **STKOF**

**ARMv8** 架构的内核直接提供了硬件的 **栈溢出** 检测，该错误被归属于 **UsageFault** 类型。

我们可以通过设置两个 **32-bit Stack Pointer Limit Registers，MSPLIM and PSPLIM** 来描述需要监控的栈空间。

当发生栈溢出错误时，芯片会进入 **UsageFault Exception**。

![UsageFault Status Register][1]

![STKOF][2]

完整例子可以参见 [ARM_CM33_NTZ][3]

### MPU

**MPU (Memory Protection Unit)** 提供了内存区域保护功能，能防止内存资源被其它或自身任务非法访问。因此 **MPU** 能检测到自身的 **栈溢出** 错误。

 [1]: ./images/ufsr.jpg
 [2]: ./images/stkof.jpg
 [3]: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/ARMv8M/non_secure/portable/GCC/ARM_CM33_NTZ/
