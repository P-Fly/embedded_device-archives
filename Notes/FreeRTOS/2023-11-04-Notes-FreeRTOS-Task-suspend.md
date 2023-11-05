# 任务的挂起与恢复

## vTaskSuspend

需要注意该函数没有累积性，即使多次调用 **vTaskSuspend**，但只要调用一次 **vTaskResume** 就能恢复任务。

### 原型

```
void vTaskSuspend( TaskHandle_t xTaskToSuspend );
```

### 参数

> TaskHandle_t xTaskToSuspend
 
需要挂起的任务句柄。

## vTaskResume

### 功能

恢复一个被挂起的任务。

### 原型

```
void vTaskResume( TaskHandle_t xTaskToResume );
```

### 参数

> TaskHandle_t xTaskToResume

需要恢复的任务句柄。

## xTaskResumeFromISR

### 功能

在中断中恢复一个被挂起的任务。

### 原型

```
BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume );
```

### 参数

> TaskHandle_t xTaskToResume

需要恢复的任务句柄。

### 返回值

如果该函数返回非0，表明应该发起一次上下文切换（有高优先级任务或同优先级任务需要切换）。
