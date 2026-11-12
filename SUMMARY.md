# 目录

- [仓库说明](README.md)

---

- 架构手册
    - ARM
        - [ABI](Architecture/ARM/ABI/)
        - [v7-M](Architecture/ARM/v7-M/)
        - [v8-M](Architecture/ARM/v8-M/)

---

- 书籍
    - [Cortex-M](Books/Cortex-M/)

---

- 笔记

    - FreeRTOS
        - [笔记说明与源码参考](Notes/FreeRTOS/README.md)

        - 链表
            - [链表](Notes/FreeRTOS/2023-09-05-Notes-FreeRTOS-List.md)

        - 任务管理
            - [任务介绍](Notes/FreeRTOS/2023-09-24-Notes-FreeRTOS-Task-introduction.md)
            - [任务的创建与删除](Notes/FreeRTOS/2023-09-25-Notes-FreeRTOS-Task-create-and-delete.md)
            - [调度器的启动](Notes/FreeRTOS/2023-10-08-Notes-FreeRTOS-Task-start-scheduler.md)
            - [调度器的挂起](Notes/FreeRTOS/2023-10-29-Notes-FreeRTOS-Task-suspend-scheduler.md)
            - [系统节拍与 SysTick](Notes/FreeRTOS/2023-10-29-Notes-FreeRTOS-Task-systick.md)
            - [任务切换](Notes/FreeRTOS/2023-10-20-Notes-FreeRTOS-Task-context-switch.md)
            - [任务的延时](Notes/FreeRTOS/2023-11-04-Notes-FreeRTOS-Task-delay.md)
            - [任务的挂起](Notes/FreeRTOS/2023-11-04-Notes-FreeRTOS-Task-suspend.md)
            - [空闲任务](Notes/FreeRTOS/2023-11-06-Notes-FreeRTOS-Task-idle-function.md)

        - 任务间通信
            - [队列](Notes/FreeRTOS/2024-10-06-Notes-FreeRTOS-ITC-queue.md)
            - [队列集](Notes/FreeRTOS/2024-10-07-Notes-FreeRTOS-ITC-queue-set.md)
            - [信号量和互斥量](Notes/FreeRTOS/2024-10-08-Notes-FreeRTOS-ITC-semaphore-and-mutex.md)
            - [任务通知](Notes/FreeRTOS/2024-10-15-Notes-FreeRTOS-ITC-task-notifications.md)
            - [流缓冲和消息缓冲](Notes/FreeRTOS/2024-10-15-Notes-FreeRTOS-ITC-stream-buffer.md)

        - 软件定时器
            - [软件定时器](Notes/FreeRTOS/2024-10-17-Notes-FreeRTOS-Timer-software-timers.md)

        - 内存管理
            - [内存管理](Notes/FreeRTOS/2023-10-21-Notes-FreeRTOS-Memory-management.md)

        - 其他主题
            - [栈类型](Notes/FreeRTOS/2023-09-26-Notes-FreeRTOS-Stack-type.md)
            - [中断优先级](Notes/FreeRTOS/2023-10-22-Notes-FreeRTOS-Interrupt-priority.md)
            - [特殊链表 - xPendingReadyList](Notes/FreeRTOS/2023-10-22-Notes-FreeRTOS-List-xpendingreadylist.md)

    - Keil
        - [如何生成反汇编文件](Notes/Keil/2023-09-24-Notes-Keil-How-to-output-disassembly-file.md)
        - [如何配置并行构建](Notes/Keil/2023-09-24-Notes-Keil-How-to-parallel-build.md)
        - [如何计算 Flash 和 RAM 的大小](Notes/Keil/2023-09-24-Notes-Keil-How-to-calculate-flash-and-memory-size.md)

    - 编码
        - [Base64 介绍](Notes/Encode/2024-01-20-Notes-Encode-Base64-introduction.md)

    - Zephyr
        - 入门
            - [入门指南](Notes/Zephyr/2025-08-13-Notes-Zephyr-Getting-started-guide.md)

    - 内存安全
        - [Stack Canary 机制](Notes/MemorySafety/StackCanary/2026-11-12-Notes-MemorySafety-Stack-canary.md)
        - [如何检测栈溢出](Notes/MemorySafety/StackOverflowDetection/2024-11-09-Notes-MemorySafety-How-to-check-the-stack-overflow.md)

    - 设计方案
        - [如何压缩日志以减少数据传输](Notes/Design/LogCompression/2024-10-26-Notes-Design-How-to-compress-logs-to-reduce-data-transmission.md)
