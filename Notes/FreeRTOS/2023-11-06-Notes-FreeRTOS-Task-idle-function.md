# 空闲任务

## prvIdleTask

### 功能

当调度器启动时，内核将自动创建空闲任务。它是以最低优先级创建的，以确保始终有一个任务能够运行。主要功能如下：

- 清理自删除任务留在 `xTasksWaitingTermination` 中的内核资源：调用 `prvDeleteTCB`，按分配方式释放动态分配的任务栈和 TCB；静态分配的存储不由内核释放。Idle 长期得不到运行机会时，这项清理也会被推迟。

- 提供低功耗支持。

任务持有的应用资源不随任务删除而自动回收，例如应用申请的缓冲区、外设和互斥量，应由应用安排释放或移交。

### 原型

```c
void prvIdleTask( void *pvParameters );
```

### 流程

![prvIdleTask][1]

图示仅概览 Idle 循环：协作式调度时 Idle 主动让出 CPU；抢占式调度下，只有 `configIDLE_SHOULD_YIELD == 1` 且同优先级有其他就绪任务时，才在这一分支主动让出 CPU。

设置 `configUSE_IDLE_HOOK == 1` 时，Idle 调用 `vApplicationIdleHook`。它运行在 Idle 任务上下文，不能调用可能使任务阻塞的 API，例如延时、带非零等待时间的队列或信号量操作；处理应保持短小，避免拖延下一轮清理和睡眠判断。

## 低功耗 tickless 模式

### 启用条件与边界

普通 Idle hook 是后台处理入口；tickless 是在预计空闲期间抑制周期 tick、醒来后补偿内核时间的机制；芯片的深睡模式则涉及电源域、时钟、存储保持和唤醒源配置，三者不能等同。执行 WFI 也不自动表示进入芯片的深睡模式。

内核在 `configUSE_TICKLESS_IDLE != 0` 时编译 tickless 路径。下面具体说明 **RVDS/ARM_CM4F、非 MPU、Arm Compiler 5** 的默认实现，其 `configUSE_TICKLESS_IDLE == 1` 分支使用 SysTick 和 Cortex-M4F 的 WFI；自定义实现可以使用其他非零配置值及低功耗计时源，不能直接套用这套寄存器处理。

### 睡眠与唤醒流程

1. Idle 通过 `prvGetExpectedIdleTime` 估算下一次任务超时前的空闲 tick 数。达到 `configEXPECTED_IDLE_TIME_BEFORE_SLEEP` 后挂起调度器，再估算一次；`configPRE_SUPPRESS_TICKS_AND_SLEEP_PROCESSING( xExpectedIdleTime )` 可以将该值置零取消本次尝试。再次达到门槛才调用 `portSUPPRESS_TICKS_AND_SLEEP`。

2. port 将预期时长限制在 SysTick 24 位计数器可表达的范围内，使用 `__disable_irq` 设置 PRIMASK，配合屏障后调用 `eTaskConfirmSleepModeStatus`。存在待就绪任务、待切换请求或 `xPendedTicks` 时返回 `eAbortSleep`，恢复中断并放弃睡眠。挂起调度器本身不屏蔽中断；睡眠入口的屏蔽与再次确认防止“已确认空闲、ISR 又使任务就绪、随后仍睡下”的竞态。

3. 确认可睡眠后暂停 SysTick，根据当前剩余计数和预期 tick 数设置较长重载值。若原 SysTick 中断已挂起，会清除该挂起并修正重载值；同时扣除停止计数器带来的估计补偿，再启动 SysTick。这里并非每次都切换到外部低功耗时钟。

4. 调用 pre-sleep 宏，必要时执行 WFI，然后调用 post-sleep 宏。此时仍处于 PRIMASK 屏蔽区；屏蔽中断服务不等于禁止符合条件的挂起中断唤醒 WFI。post-sleep 完成后开放中断，让唤醒 ISR 执行，再次屏蔽中断以计算时间和恢复计时器。

5. 若 SysTick 已计到零，port 将完整补偿量设为 `xExpectedIdleTime - 1`，留一个 tick 由正常 tick 处理路径计入；若由其他中断提前唤醒，则从剩余计数计算已过去的完整 tick 数。两条路径都要处理不足一个 tick 的余量并恢复常规 SysTick 周期，再调用 `vTaskStepTick( ulCompleteTickPeriods )` 推进内核 tick，退出时开放中断。

6. 返回 Idle 后调用 `xTaskResumeAll`，处理调度器挂起期间累积的 tick 和就绪任务。`xPendedTicks` 参与挂起期间的 tick 记账，不能用“将整个睡眠长度写入该变量”代替 port 的计数补偿、`vTaskStepTick` 和正常 tick 处理。

### Sleep hooks 参数

`configPRE_SLEEP_PROCESSING( xModifiableIdleTime )` 接收预期空闲时间的副本；若宏自己完成等待，可将该副本置零，阻止 port 再执行一次 WFI。`configPOST_SLEEP_PROCESSING( xExpectedIdleTime )` 使用原预期值，它不是实测睡眠时长；不要修改后续补偿仍要使用的原值。两者都是宏调用，不是固定的函数原型，也不同于 Idle hook；应保持短小，不执行阻塞等待，并按芯片要求处理时钟与唤醒配置。

### 有界算例

仅按上述 SysTick 到零分支推导：假设当前 tick 为 100，最近超时点为 105，预期空闲 5 tick，且没有其他中断提前唤醒。port 用 `vTaskStepTick( 4 )` 将 tick 推进到 104，剩余一个 tick 通过正常 tick 处理到 105，并在恢复调度时处理到期任务。这是源码分支的计数模型，不是硬件实测；提前唤醒、计数器停止补偿和时钟误差需要按实际 port 与芯片核对。

### 源码依据

- [tasks.c：prvIdleTask、prvCheckTasksWaitingTermination、eTaskConfirmSleepModeStatus、vTaskStepTick](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)。
- [portable/RVDS/ARM_CM4F/port.c：vPortSuppressTicksAndSleep、vPortSetupTimerInterrupt](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/port.c)。

 [1]: ./images/prvIdleTask.jpg
