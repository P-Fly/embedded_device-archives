# 系统节拍与 SysTick

SysTick 作为系统节拍源时，调度器未挂起的情况下，内核在每个节拍到来后更新 tick 计数，检查是否有延时到期的任务需要**解除阻塞并进入就绪态**，并判断是否需要请求任务切换。

任务主动让出处理器、进入阻塞状态，或中断解除其他任务的阻塞时，也可能触发任务切换。

## SysTick 配置

`portable/RVDS/ARM_CM4F` 的默认 `vPortSetupTimerInterrupt()` 实现使用以下宏配置周期性节拍：

| 配置项 | 含义 |
| --- | --- |
| `configCPU_CLOCK_HZ` | CPU 内核的实际时钟频率，单位为 Hz，需与时钟源及分频后的结果一致 |
| `configTICK_RATE_HZ` | 每秒期望产生的系统节拍数，单位为 Hz |
| `configSYSTICK_CLOCK_HZ` | 选择另一条 SysTick 输入时声明其实际频率，单位为 Hz；未定义时默认使用 `configCPU_CLOCK_HZ` |

在该 port 中，是否定义 `configSYSTICK_CLOCK_HZ` 还决定 `SysTick CTRL.CLKSOURCE` 的取值：

| 配置方式 | 时钟选择 | 参与重装值计算的频率 |
| --- | --- | --- |
| 未定义 `configSYSTICK_CLOCK_HZ` | `CLKSOURCE = 1`，处理器时钟 | `configCPU_CLOCK_HZ` |
| 已定义 `configSYSTICK_CLOCK_HZ` | `CLKSOURCE = 0`，芯片提供的另一条 SysTick 输入 | `configSYSTICK_CLOCK_HZ` |

宏的数值必须匹配所选输入；芯片时钟树由平台初始化代码配置。其他 port 或应用自定义的节拍初始化，应以各自的定时器配置为准。

### 重装值与节拍周期

SysTick 从重装值递减到零，一个周期包含 `LOAD + 1` 个输入时钟周期。上述默认初始化按整数除法计算重装值：

$$
\mathrm{LOAD} = \left\lfloor \frac{f_{\mathrm{SysTick}}}{\texttt{configTICK\_RATE\_HZ}} \right\rfloor - 1
$$

其中 $f_{\mathrm{SysTick}}$ 为所选 SysTick 输入的实际频率。实际节拍周期为：

$$
T_{\mathrm{tick}} = \frac{\mathrm{LOAD} + 1}{f_{\mathrm{SysTick}}}
$$

SysTick 计数器为 24 位，`LOAD` 的最大值为 `0xFFFFFF`。配置时需确保节拍频率大于零，计算得到的重装值有效且不超过该上限；不能整除时，整数截断会使实际频率与期望值存在偏差。以上计算针对正常周期性节拍，tickless idle 会临时调整计数安排。

### STM32WB55 时钟示例

下图中，STM32WB55 的 CPU1 使用 HCLK1，SysTick 的处理器时钟输入也来自 HCLK1；另一条输入来自 HCLK1/8。这个分频关系由该芯片实现决定。

![Clock Tree][1]

例如，HCLK1 为 32 MHz、`configTICK_RATE_HZ` 为 1000 Hz 时，两条路径的配置如下：

| SysTick 输入 | 配置 | 输入频率 | `LOAD` | 节拍周期 |
| --- | --- | --- | --- | --- |
| HCLK1 | `configCPU_CLOCK_HZ = 32000000`，不定义 `configSYSTICK_CLOCK_HZ` | 32 MHz | 31999 | 1 ms |
| HCLK1/8 | `configCPU_CLOCK_HZ = 32000000`，`configSYSTICK_CLOCK_HZ = 4000000` | 4 MHz | 3999 | 1 ms |

## SysTick 中断

### 功能

上述 Cortex-M4F port 在中断屏蔽保护下执行 `xTaskIncrementTick()`；其返回非零时置位 PendSV 的 pending 请求，由 PendSV 处理上下文切换。PendSV 的实际响应时机还受异常优先级及屏蔽状态影响。

### 原型

```c
void xPortSysTickHandler( void );
```

## xTaskIncrementTick

### 功能

更新系统节拍计数，处理延时到期的任务，并判断是否需要请求任务切换。调度器挂起期间先累计 `xPendedTicks`，待恢复调度器时补处理积累的节拍。

### 原型

```c
BaseType_t xTaskIncrementTick( void );
```

### 返回值

返回 `pdTRUE` 表示需要请求任务切换。启用抢占调度（`configUSE_PREEMPTION = 1`）时，延时到期使更高优先级任务就绪，或内核已有待处理的切换请求，都可能得到该结果；同时启用时间片（`configUSE_TIME_SLICING = 1`）且当前优先级有多个就绪任务时，还会请求同优先级任务轮转。每个 tick 都会执行节拍处理，是否请求切换由这些条件决定。

### 流程

![xTaskIncrementTick][2]

## 关于计数器的溢出和延时链表的交换

1. 内核使用 `xTickCount` 记录系统节拍。其宽度取决于 `TickType_t` 和内核配置；以 32 位无符号计数为例，取值范围是 $0 \leq xTickCount \leq 2^{32} - 1$。

2. 对于有限 tick 延时，唤醒时间按 `TickType_t` 的无符号加法计算：`xTimeToWake = xTickCount + xTicksToDelay`。如果结果没有回绕，即 `xTimeToWake >= xTickCount`，任务进入 `pxDelayedTaskList`。

3. 如果结果发生回绕，即 `xTimeToWake < xTickCount`，任务进入 `pxOverflowDelayedTaskList`，等待 tick 计数回绕后再检查其唤醒时间。两种情况下，存入 `xTimeToWake` 的值都仍在 `TickType_t` 的取值范围内。

4. 当 `xTickCount` 回绕到零时，交换两个延时链表的指针，不需要重新计算已记录的唤醒时间。

例如，32 位 tick 下，当前 `xTickCount = 0xFFFFFFF0`，延时 20 个 tick 后的唤醒时间为 `0x00000004`。该任务先进入溢出延时链表；tick 计数回绕并交换链表后，在计数到 4 时解除阻塞。

![Switch Delayed Lists][3]

## 源码依据

- [Cortex-M4F port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/RVDS/ARM_CM4F/port.c)：`configSYSTICK_CLOCK_HZ` 的默认值与时钟源选择、`vPortSetupTimerInterrupt()`、`xPortSysTickHandler()`。
- [tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)：`xTaskIncrementTick()` 及延时链表的处理。

 [1]: ./images/clock_tree.jpg
 [2]: ./images/xTaskIncrementTick.jpg
 [3]: ./images/switch_delayed_lists.jpg
