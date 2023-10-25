# 任务切换

任务切换的接口如下图：

![task_yield][1]

由上图可知，内核中有3个方式可以触发任务切换：

 1. 在 **Thread mode** 下通过系统调用 **taskYIELD** / **taskYIELD_IF_USING_PREEMPTION** 执行任务切换。

 2. 在 **Handler mode** 下通过系统调用 **portYIELD_FROM_ISR** / **portEND_SWITCHING_ISR** 执行任务切换。

 3. 内核会在系统时钟中断 **xPortSysTickHandler** 中自动执行任务切换。

## 触发任务切换

无论是在系统调用场景还是系统时钟中断场景，任务切换的方式都是一致的：通过设置 **ICSR** 寄存器来请求任务切换。此时，系统产生 **PendSV Exception**。

```
portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
```

## PendSV Exception

### 原型

```
void xPortPendSVHandler( void );
```

### 流程

进入 **PendSV Exception**，此时处于 **Handler mode** 使用 **MSP**。

```
PRESERVE8
````

声明当前函数的栈需要 **8Byte** 对齐。

```
mrs r0, psp
isb
ldr r3, =pxCurrentTCB
ldr r2, [r3]
````

对寄存器进行赋值：

 1. **r0** = **psp**

 2. **r3** = **&pxCurrentTCB**

 3. **r2** = **&TCB_t**

```
stmdb r0!, {r4-r11, r14}
````

将 **r4 - r11, r14** 寄存器的值存入 **用户堆栈**。

```
str r0, [r2]
````

更新 **pxCurrentTCB->pxTopOfStack**。

```
stmdb sp!, {r0, r3}
mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
msr basepri, r0
dsb
isb
bl vTaskSwitchContext
mov r0, #0
msr basepri, r0
ldmia sp!, {r0, r3}
````

执行函数 **vTaskSwitchContext**：

 1. 备份 **r0, r3** 寄存器。

 2. 进入临界区，相当于调用了 **vPortRaiseBASEPRI**。

 3. 调用 **vTaskSwitchContext** 切换到下一个需要运行的任务，即更新 **pxCurrentTCB** 的值。

 4. 退出临界区，相当于调用了 **vPortClearBASEPRIFromISR**。

 5. 恢复 **r0, r3** 寄存器。

```
ldr r1, [r3]
ldr r0, [r1]
```

获取新任务的栈顶地址：**r0 = pxCurrentTCB->pxTopOfStack**。

```
ldmia r0!, {r4-r11, r14}
```

恢复 **R4 - R11, R14** 寄存器。

```
msr psp, r0
isb
```

重新设置 **PSP** 寄存器。

```
bx r14
```

从 **PendSV Exception** 返回。

 [1]: ./images/task_yield.jpg
