# 任务的延时

## vTaskDelay

### 功能

将任务阻塞一段时间。

### 原型

```
void vTaskDelay( const TickType_t xTicksToDelay );
```

### 参数

> const TickType_t xTicksToDelay

需要阻塞的系统时钟节拍数。可以用 **pdMS_TO_TICKS** 将需要延时的毫秒值转换为系统时钟的节拍数。

## vTaskDelayUntil

### 功能

将任务阻塞到指定的时间。该函数可以被周期性的任务调用，以确保恒定的执行频率。

该函数与 **vTaskDelay** 最大的区别在于：**vTaskDelayUntil** 不会因为任务本身运行时间的长短影响到任务周期调度的准确性。

### 原型

```
void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement );
```

### 参数

> TickType_t * const pxPreviousWakeTime

指向一个变量，该变量保存任务最后一次解除阻塞的时间。第一次使用前，该变量必须初始化为当前时间。

> const TickType_t xTimeIncrement

周期循环的时钟节拍数。可以用 **pdMS_TO_TICKS** 将需要延时的毫秒值转换为系统时钟的节拍数。

## xTaskAbortDelay

### 功能

强制任务退出阻塞状态，进入就绪状态。

### 原型

```
BaseType_t xTaskAbortDelay( TaskHandle_t xTask );
```

### 参数

> TaskHandle_t xTask

需要操作的任务句柄。

### 返回值

如果任务没有处于阻塞状态，则返回假。否则返回真。
