# 队列集

**队列集** 提供了一种机制，允许任务 **同时阻塞在多个队列或信号量** 的读取操作中。**队列** 和 **信号量** 被划分为集合，不管哪一个消息到来，都可以让任务退出阻塞状态。

[Blocking on Multiple RTOS Objects][1]

## 使用方法

 1. 使用 **xQueueCreateSet** 显示创建队列集。

 2. 使用 **xQueueAddToSet** 将队列或信号量添加至集合中。

 3. 使用 **xQueueSelectFromSet** 或 **xQueueSelectFromSetFromISR** 监视队列集的状态改变。

 4. 如果集合接收到队列消息，则 **步骤3** 返回需要读取的队列的句柄。最后通过 **xQueueReceive** 或 **xQueueSemaphoreTake** 读取具体的队列消息。

![QueueSet Receive][2]

**队列集** 自身就是一个队列，其存储的是 **队列** 或 **信号量** 的句柄。当向 **队列** 或 **信号量** 发送消息时，系统会通过 **prvNotifyQueueSetContainer** 接口，将对应的句柄放置到 **队列集** 内。这样任务就会被 **队列集** 唤醒，并通过两次引用获取具体的 **队列** 或 **信号量** 信息。

## 替代方案

可以定义一个结构体，通过一个成员来保存事件类型，另一个成员保存与事件相关联的数据或指针。最后使用 **单个队列** 来发送和接收定义的消息。

 [1]: https://www.freertos.org/Pend-on-multiple-rtos-objects.html
 [2]: ./images/QueueSet_receive.jpg
