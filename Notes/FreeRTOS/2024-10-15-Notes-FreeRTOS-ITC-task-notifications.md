# 任务通知

每个任务都有一个 **通知数组 ulNotifiedValue[]** 和 **状态数组 ucNotifyState[]**。每个任务都有 **挂起** 或 **非挂起** 的通知状态。

**任务通知** 是直接发送到任务的事件，而不会通过中间对象间接发送至任务。

向任务发送 **任务通知** 会将目标任务设为 **挂起** 状态。任务也可以阻塞 **任务通知**，以等待通知状态变为 **挂起**。

 - 该功能从 **v8.2.0** 开始可用。

 - 从 **v10.4.0** 起支持多条通知。

### 通知值更新方式

 - 覆盖原值，无论接收任务是否读取被覆盖的值。

 - 覆盖原值，但前提是接收任务已读取被覆盖的值。

 - 在值中设置一个或多个位。

 - 对值进行增量。

### 使用限制

 - 任务通知只能有一个接收任务（多发单收模式）。

 - 接收任务可在阻塞状态下等待通知，但发送任务不能在阻塞状态下等待发送完成。

## 样例

### 替代二进制信号量

| API | 功能 | 备注 |
| :--- | :--- | :--- |
| ulTaskNotifyTake | 等待任务通知 | **参数 xClearCountOnExit = True** |
| ulTaskNotifyTakeIndexed | 等待任务通知 | **参数 xClearCountOnExit = True** |
| xTaskNotifyGive | 发出任务通知 |  |
| xTaskNotifyGiveIndexed | 发出任务通知 |  |
| vTaskNotifyGiveIndexedFromISR | 发出任务通知 |  |
| vTaskNotifyGiveFromISR | 发出任务通知 |  |

### 替代计数信号量

| API | 功能 | 备注 |
| :--- | :--- | :--- |
| ulTaskNotifyTake | 等待任务通知 | **参数 xClearCountOnExit = False** |
| ulTaskNotifyTakeIndexed | 等待任务通知 | **参数 xClearCountOnExit = False** |
| xTaskNotifyGive | 发出任务通知 |  |
| xTaskNotifyGiveIndexed | 发出任务通知 |  |
| vTaskNotifyGiveIndexedFromISR | 发出任务通知 |  |
| vTaskNotifyGiveFromISR | 发出任务通知 |  |

### 替代事件组

| API | 功能 | 备注 |
| :--- | :--- | :--- |
| xTaskNotifyWait | 等待任务通知 |  |
| xTaskNotify | 发出任务通知 |  |
| xTaskNotifyFromISR | 发出任务通知 |  |

### 替代邮箱

| API | 功能 | 备注 |
| :--- | :--- | :--- |
| xTaskNotifyWait | 等待任务通知 |  |
| xTaskNotify | 发出任务通知 |  |
| xTaskNotifyIndexed | 发出任务通知 |  |
| xTaskNotifyFromISR | 发出任务通知 |  |
| xTaskNotifyIndexedFromISR | 发出任务通知 |  |
