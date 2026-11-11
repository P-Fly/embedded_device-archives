# 链表

在 **FreeRTOS** 中，大量使用了 **List** 作为最基本的一种数据结构，比如：

- **调度器**：使用链表跟踪任务的不同状态。

- **消息队列**：使用链表管理等待发送或接收的任务；消息数据存放在队列缓冲区中。

- **延时队列**：使用链表跟踪任务需要多久才被处理。

因此，我们需要分析下 **FreeRTOS** 中的链表是如何组织的。

## 基本数据结构

**FreeRTOS** 的链表使用了 `List_t` 和 `ListItem_t` 这两组数据结构，其中：

以下结构体为节选，`...` 表示省略的成员。`xListEnd` 是内嵌的链表尾哨兵项，不是尾指针；其值为 `portMAX_DELAY`，不计入 `uxNumberOfItems`。

- `List_t` 用于管理该组链表，相当于一个链表头。

    ```c
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

- `ListItem_t` 用于记录链表的每个数据单元。

     ```c
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

因为 `ListItem_t` 中的部分数据在 `List_t` 中并不需要，因此 **FreeRTOS** 新增了一个数据结构 `MiniListItem_t`，用于节省内存。

`pvOwner` 指向拥有该链表项的对象，通常是包含它的任务控制块；`pxContainer` 指向该项当前所在的 `List_t`，未入链或移除后为 `NULL`。两者分别回答“这个项属于哪个对象”和“这个项挂在哪条链表上”。

## 实例

![list instance][1]

总结：**FreeRTOS** 使用双向链表将每一个 `ListItem_t` 链接在一起，由 `List_t` 负责管理。

图中的 `xListEnd.pxNext` 指向首个数据项，`xListEnd.pxPrevious` 指向最后一个数据项，两端都连接回哨兵，形成环形结构。空链表中这两个指针都指向哨兵自身。

`pxIndex` 是环形遍历的游标。`listGET_OWNER_OF_NEXT_ENTRY` 先将其移到下一项，遇到哨兵则跳过，再返回该项的 `pvOwner`；连续调用会循环访问各数据项。该操作要求链表非空。

## 常用函数接口

### `vListInitialise`

初始化链表头 `List_t`。

此时计数为 0，`pxIndex` 指向 `xListEnd`，哨兵的前后指针都指向自身。

### `vListInitialiseItem`

初始化链表项 `ListItem_t`。

将 `pxContainer` 置为 `NULL`；插入前还需要由调用方设置所需的 `xItemValue` 和 `pvOwner`。

### `vListInsertEnd`

不需要排序，而是在当前迭代器 `pxIndex` 的位置前方插入 `ListItem_t`。

*比如: 链表上有三个数，分别为 [3] [4] [5]，迭代器指向了 [4]，需要插入的数据为 [6]，则最终插入后的数据为 [3], [6], [4], [5]。*

插入不会移动 `pxIndex`。这里的 End 是相对于遍历游标的插入位置，不保证按 `xItemValue` 排序，也不一定插在哨兵之前。以上例从游标 `[4]` 继续遍历，会依次访问 `[5], [3], [6], [4]`。

### `vListInsert`

以升序方式插入 `ListItem_t`。

*比如: 链表上有三个数，分别为 [3] [4] [5]，需要插入的数据为 [4]，则最终插入后的数据为 [3], [4], [4 - New], [5]。*

新项插在所有等值项之后，因此仍在链表中的等值项保持插入先后顺序（FIFO）。值为 `portMAX_DELAY` 的项会插在哨兵之前，避免查找时绕过同值哨兵后继续循环。

### `uxListRemove`

删除链表项。

通过该项的 `pxContainer` 找到所属链表，重连相邻项，将 `pxContainer` 清为 `NULL`，计数减一，并返回剩余项数。若 `pxIndex` 正指向被删除项，则先回退到其前一项，使下一次遍历仍从原来的后一项继续；删除其他项不移动游标。

例如从 `[3], [4], [5]` 中删除游标所指的 `[4]`，游标回退到 `[3]`，下一次遍历返回 `[5]`；删除最后一个数据项后，游标回到哨兵。

## 其它

我们可以使用 `configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES` 宏，在数据结构中放置一些预设的数据，并在需要时通过检测这些数据来确定这部分内存是否被破坏。

 [1]: ./images/list_instance.jpg
