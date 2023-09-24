# 链表

在 **FreeRTOS** 中，大量使用了 **List** 作为最基本的一种数据结构，比如:

 - **调度器**: 使用链表跟踪任务的不同状态。

 - **消息队列**: 使用链表跟踪消息的发送和接收。

 - **延时队列**: 使用链表跟踪任务需要多久才被处理。

因此，我们需要分析下 **FreeRTOS** 中的链表是如何组织的。

## 基本数据结构

**FreeRTOS** 的链表使用了 **List_t** 和 **ListItem_t** 这两组数据结构，其中:

 - **List_t** 用于管理该组链表，相当于一个链表头。

    ```
    /*
     * Definition of the type of queue used by the scheduler.
     */
    typedef struct xLIST
    {
        ...
        /* 列表项的个数 */
        volatile UBaseType_t uxNumberOfItems;

        /* 用于遍历链表项的迭代器 */
        ListItem_t * configLIST_VOLATILE pxIndex;

        /* 链表尾指针，该链表项的值为该组链表的最大可能值 */
        MiniListItem_t xListEnd;
        ...
    } List_t;
    ```

 - **ListItem_t** 用于记录链表的每个数据单元。

    ```
    struct xLIST_ITEM
    {
        ...
        /* 该列表项的值，可以保存任务优先级、阻塞时间等信息。大部分应用中会按照升序排序 */
        configLIST_VOLATILE TickType_t xItemValue;

        /* 双向指针 */
        struct xLIST_ITEM * configLIST_VOLATILE pxNext;
        struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;

        /* 指向包含该列表项的对象 */
        void * pvOwner;

        /* 指向所属的 List_t */
        struct xLIST * configLIST_VOLATILE pxContainer;
        ...
    };
    typedef struct xLIST_ITEM ListItem_t;
    ```

因为 **ListItem_t** 中的部分数据在 **List_t** 中并不需要，因此 **FreeRTOS** 新增了一个数据结构 **MiniListItem_t**，用于节省内存。

## 实例

![list instance][1]

总结: **FreeRTOS** 使用双向链表将每一个 **ListItem_t** 链接在一起，由 **List_t** 负责管理。

## 常用函数接口

### vListInitialise

初始化链表头 **List_t**。

### vListInitialiseItem

初始化链表项 **ListItem_t**。

### vListInsertEnd

不需要排序，而是在当前迭代器 **pxIndex** 的位置前方插入 **ListItem_t**。

*比如: 链表上有三个数，分别为 [3] [4] [5]，迭代器指向了 [4]，需要插入的数据为 [6]，则最终插入后的数据为 [3], [6], [4], [5]。*

### vListInsert

以升序方式插入 **ListItem_t**。

*比如: 链表上有三个数，分别为 [3] [4] [5]，需要插入的数据为 [4]，则最终插入后的数据为 [3], [4], [4 - New], [5]。*

### uxListRemove

删除链表项。

## 其它

我们可以使用 **configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES** 宏，在数据结构中放置一些预设的数据，并在需要时通过检测这些数据来确定这部分内存是否被破坏。

 [1]: ./images/list_instance.jpg
