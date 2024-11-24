# Summary

- [引言](README.md)

---

- 架构手册
    - ARM
        - [ABI](Architecture/ARM/ABI/)
        - [v7-M](Architecture/ARM/v7-M/)
        - [v8-M](Architecture/ARM/v8-M/)

---

- 数据手册
    - [STM32WB55](Datasheet/STM32WB55/)

---

- Notes
    - FreeRTOS
        - List
            - [链表](Notes/FreeRTOS/2023-09-05-Notes-FreeRTOS-List.md)

        - Task
            - [任务介绍](Notes/FreeRTOS/2023-09-24-Notes-FreeRTOS-Task-introduction.md)
            - [任务的创建与删除](Notes/FreeRTOS/2023-09-25-Notes-FreeRTOS-Task-create-and-delete.md)
            - [调度器的启动](Notes/FreeRTOS/2023-10-08-Notes-FreeRTOS-Task-start-scheduler.md)
            - [调度器的挂起](Notes/FreeRTOS/2023-10-29-Notes-FreeRTOS-Task-suspend-scheduler.md)
            - [系统时钟](Notes/FreeRTOS/2023-10-29-Notes-FreeRTOS-Task-systick.md)
            - [任务的调度](Notes/FreeRTOS/2023-10-20-Notes-FreeRTOS-Task-context-switch.md)
            - [任务的延时](Notes/FreeRTOS/2023-11-04-Notes-FreeRTOS-Task-delay.md)
            - [任务的挂起](Notes/FreeRTOS/2023-11-04-Notes-FreeRTOS-Task-suspend.md)
            - [空闲任务](Notes/FreeRTOS/2023-11-06-Notes-FreeRTOS-Task-idle-function.md)

        - Inter-Task Communication
            - [队列](Notes/FreeRTOS/2024-10-06-Notes-FreeRTOS-ITC-queue.md)
            - [队列集](Notes/FreeRTOS/2024-10-07-Notes-FreeRTOS-ITC-queue-set.md)
            - [信号量和互斥量](Notes/FreeRTOS/2024-10-08-Notes-FreeRTOS-ITC-semaphore-and-mutex.md)
            - [任务通知](Notes/FreeRTOS/2024-10-15-Notes-FreeRTOS-ITC-task-notifications.md)
            - [流缓冲和消息缓冲](Notes/FreeRTOS/2024-10-15-Notes-FreeRTOS-ITC-stream-buffer.md)

        - Software Timer
            - [软件定时器](Notes/FreeRTOS/2024-10-17-Notes-FreeRTOS-Timer-software-timers.md)

        - Memory Management
            - [内存管理](Notes/FreeRTOS/2023-10-21-Notes-FreeRTOS-Memory-management.md)

        - Misc
            - [栈类型](Notes/FreeRTOS/2023-09-26-Notes-FreeRTOS-Stack-type.md)
            - [中断优先级](Notes/FreeRTOS/2023-10-22-Notes-FreeRTOS-Interrupt-priority.md)
            - [特殊链表 - xPendingReadyList](Notes/FreeRTOS/2023-10-22-Notes-FreeRTOS-List-xpendingreadylist.md)

    - Keil
        - [如何生成反汇编文件](Notes/Keil/2023-09-24-Notes-Keil-How-to-output-disassembly-file.md)
        - [如何配置并行编程](Notes/Keil/2023-09-24-Notes-Keil-How-to-parallel-build.md)
        - [如何计算Flash和RAM的大小](Notes/Keil/2023-09-24-Notes-Keil-How-to-calculate-flash-and-memory-size.md)

    - Encode
        - [Base64介绍](Notes/Encode/2024-01-20-Notes-Encode-Base64-introduction.md)

- Design
    - [如何压缩日志以减少数据传输](Design/compress_logs/2024-10-26-Design-How-to-compress-logs-to-reduce-data-transmission.md)
    - [如何检测栈溢出](Design/check_stack_overflow/2024-11-09-Design-How-to-check-the-stack-overflow.md)
