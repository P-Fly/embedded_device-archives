# 流缓冲和消息缓冲

流缓冲和消息缓冲通过拷贝传输数据，可用于任务之间或任务与 ISR 之间的通信。发送成功后，发送方可复用已复制数据的源存储区；接收方将数据复制到自己提供的存储区。两者都按 **单写者、单读者** 设计。

## 流缓冲

流缓冲本身不记录消息边界。一次发送的数据可以分多次读取，多次发送的数据也可以合并读取。应用若需要报文边界，须自行分帧，或使用消息缓冲。

| 操作 | 任务接口 | 返回值与等待规则 |
| --- | --- | --- |
| 发送 | `xStreamBufferSend()` | 返回实际复制的字节数，可能小于请求长度；无空间且不能继续等待时返回 `0`。 |
| 接收 | `xStreamBufferReceive()` | 返回实际复制的字节数，不超过接收区长度；无数据可读时返回 `0`。 |
| ISR 发送/接收 | `xStreamBufferSendFromISR()` / `xStreamBufferReceiveFromISR()` | 不阻塞，返回实际复制的字节数；另通过 `pxHigherPriorityTaskWoken` 报告是否需要请求切换。 |

发送时，若当前空闲空间不足且 `xTicksToWait` 非零，任务可等待空间；请求长度超过缓冲的可用总容量时，等待目标最多为该容量。等待结束后按当时的空闲空间复制，因此超时也可能返回部分字节。调用方必须检查返回值，再决定如何处理未发送的后续数据。

### 触发级别与部分接收

`xTriggerLevelBytes` 是发送方通知已阻塞接收者的阈值，不是一次接收的固定长度，也不是每次接收前都必须满足的门槛。调用接收时只要已有数据，就立即读取最多 `xBufferLengthBytes` 字节，不为凑齐请求长度或 trigger 而等待。

只有在调用时缓冲为空且等待时间非零时，接收任务才会等待。正常发送路径在累计数据达到 trigger 后通知该等待者，使其进入就绪态；它实际执行接收时，仍按当时可读字节数和接收区长度取较小值。等待超时时也会重新检查缓冲，可能读到低于 trigger 的数据。进入就绪态不等于立即获得 CPU。

以下各行是独立的文字算例，假设没有其他读者、接收区为 8 B、trigger 为 4 B：

| 情形 | 调用时可读 | 等待期间新增 | 返回字节数 | 缓冲剩余 | 原因 |
| --- | --- | --- | --- | --- | --- |
| 已有少量数据 | 2 B | 0 B | 2 B | 0 B | 不等待 trigger。 |
| 空缓冲，发送达到阈值 | 0 B | 6 B | 6 B | 0 B | 发送方通知等待者，接收区足够。 |
| 空缓冲，少量数据后超时 | 0 B | 3 B | 3 B | 0 B | 未达到阈值，但超时时可读取现有数据。 |
| 空缓冲，一直无数据直至超时 | 0 B | 0 B | 0 B | 0 B | 没有可读数据。 |
| 已有数据超过接收区 | 10 B | 0 B | 8 B | 2 B | 返回长度受接收区限制。 |

## 消息缓冲

消息缓冲保存可变长度的消息边界，每次发送或接收一条完整消息。实现复用流缓冲，在每条消息前保存长度；前缀占用 `sizeof(configMESSAGE_BUFFER_LENGTH_TYPE)` 字节，发送长度还必须能由该类型表示。

| 操作 | 任务接口 | 返回值与完整性 |
| --- | --- | --- |
| 发送 | `xMessageBufferSend()` | 成功返回消息载荷长度；空间须同时容纳长度前缀与整条载荷。不能完成发送时返回 `0`，不提交半条消息。 |
| 接收 | `xMessageBufferReceive()` | 成功返回下一条消息的载荷长度，不包含前缀；没有消息或接收区太小时返回 `0`。 |
| ISR 发送/接收 | `xMessageBufferSendFromISR()` / `xMessageBufferReceiveFromISR()` | 不阻塞，仍遵守整条消息规则；返回载荷字节数或 `0`，唤醒标志另行处理。 |

接收区小于队首消息长度时，该消息连同长度前缀都保留在缓冲中，后续消息也不会被越过。可以用 `xMessageBufferNextLengthBytes()` 查询下一条消息的长度后准备足够大的接收区；这不能代替对读者并发访问的协调。

例如假设长度类型占 4 B，一条 10 B 消息需要 14 B 空闲空间；仅有 13 B 时，零等待发送返回 `0`。消息已在缓冲内时，提供 8 B 接收区返回 `0` 且消息保留，随后提供 10 B 接收区可一次读出 10 B，并释放含前缀在内的 14 B 空间。

## 并发、通知与 ISR

同一缓冲可以有一个写者和一个读者并行访问，写者或读者可以是任务或 ISR。多个任务或多个 ISR 轮流写入也属于多写者，多个读取上下文同理；内核不会自动为这些同方向访问提供通用串行化。

需要多写者或多读者时，应用须串行化相应方向的全部访问。头文件给出的临界区方案要求相关收发调用的等待时间为 `0`，临界区还须覆盖实际参与竞争的上下文，并保持有界。**不能在临界区中包裹可阻塞的收发调用**。若需要多方阻塞等待，宜改用队列或由单一任务集中收发。

`stream_buffer.c` 要求 `configUSE_TASK_NOTIFICATIONS = 1` 和 `INCLUDE_xTaskGetCurrentTaskHandle = 1`。默认完成通知和阻塞等待使用非 Indexed 任务通知接口，即通知索引 `0`；该路径会清理通知状态并消费等待通知。同一任务不能把索引 `0` 同时作为独立业务事件通道，否则可能互相唤醒或丢失业务通知。需要独立业务通知时，设置 `configTASK_NOTIFICATION_ARRAY_ENTRIES > 1`，使用其他索引并成对选择 Indexed 收发接口；这不会改变缓冲默认使用的索引。

ISR 只调用对应的 `FromISR` 接口，并满足所用 port 的中断优先级限制。一次 ISR 内共用的 `xHigherPriorityTaskWoken` 应先初始化为 `pdFALSE`，把它的地址传给相关调用并累计结果，不能在后续调用前清零。返回字节数表示数据传输结果，不表示是否需要切换；ISR 结束前根据累计标志，按该 port 的 yield 宏请求切换，而不是把函数返回当作已完成任务切换。

## 源码出处

- [stream_buffer.c（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/stream_buffer.c)：`xStreamBufferSend()`、`xStreamBufferReceive()`、`prvWriteMessageToBuffer()`、`prvReadMessageFromBuffer()` 及默认完成通知宏。
- [include/stream_buffer.h（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/stream_buffer.h)：公开接口、trigger、单写者/单读者及多方访问限制。
- [include/message_buffer.h（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/message_buffer.h)：消息接口映射、完整性及接收区过小时的行为。
- [include/task.h（V10.5.1）](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/include/task.h)：`tskDEFAULT_INDEX_TO_NOTIFY` 及非 Indexed 通知接口向索引 `0` 的映射。
