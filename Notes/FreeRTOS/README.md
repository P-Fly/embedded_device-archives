# FreeRTOS 笔记说明

## 源码参考与适用范围

本目录的源码分析以官方 [FreeRTOS-Kernel V10.5.1](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V10.5.1) 为参考。原笔记实际使用的内核版本无法确认，现有代码片段不保证与该版本完全一致。

原工程的 port、编译器及配置已无法确认，不能仅根据文章日期或局部代码认定适用版本。

| 项目 | 说明 |
| --- | --- |
| 内核源码 | 官方 `FreeRTOS/FreeRTOS-Kernel` 仓库，tag `V10.5.1` |
| 调度模型 | 单核；相关结论不能直接推广到 SMP |
| port 与 CPU | 启动、异常、上下文切换、栈和低功耗实现与具体 port 和 CPU 有关，某个 Cortex-M port 的实现不适用于所有平台 |
| 编译器 | 原版本未知；汇编语法、ABI 和对齐要求与具体工具链有关 |
| FPU 与 MPU | 是否使用取决于 CPU、port 和配置，不能默认启用或关闭 |
| 内核配置 | 原始 `FreeRTOSConfig.h` 缺失；抢占、时间片、tick 宽度、软件定时器、任务挂起和通知数组等行为取决于相关配置 |

参考源码：[tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/tasks.c)、[task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h) 和 [portable](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V10.5.1/portable)。架构行为以对应 Arm 资料为依据。
