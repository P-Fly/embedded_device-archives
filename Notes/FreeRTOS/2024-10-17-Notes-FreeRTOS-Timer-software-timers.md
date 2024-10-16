# 软件定时器

**软件定时器** 能够在未来设定的时间到来时，执行用户的 **定时器回调函数**。

**定时器回调函数** 在定时器服务任务的上下文中执行。因此，**定时器回调函数永远不应该被阻塞**。

**FreeRTOS** 通过 **Timer Service Daemon** 管理软件定时器。该任务是一个单独的线程在运行，用户和软件定时器通过命令队列 **xTimerQueue** 进行通讯。当用户对软件定时器进行设置时，会向该队列发送命令，同时 **Timer Service Daemon** 会从队列中提取命令，作出相应的操作。

## Timer Service Daemon

 ![Timer service daemon][1]

## One-Shot 和 Auto-Reload

 ![One shot vs Auto-reload][2]

 [1]: ./images/rtos-timer-task-and-timer-command-queue.png
 [2]: ./images/one-shot-timer-behaviour-vs-auto-reload-timer-behaviour.png
