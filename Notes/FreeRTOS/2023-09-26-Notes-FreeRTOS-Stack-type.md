# 栈类型

本章简要描述了栈的四种类型。

## 两个分类维度

![Stack Type][1]

如图所示，可以通过两个维度区别不同类型的栈。

1. 根据栈指针指向的位置：

    - **Full**：栈指针指向最后压入数据的位置。

    - **Empty**：栈指针指向下一个将要压入数据的位置。

2. 根据栈指针移动的方向：

    - **Ascending**：栈指针由低地址向高地址生长。

    - **Descending**：栈指针由高地址向低地址生长。

这里的生长方向指压栈时的方向，出栈时方向相反。Full/Empty 描述的是 SP 与数据的相对位置，不是整个栈空间已满或已空：对于非空栈，Full 的 SP 指向已占用的栈顶单元，Empty 的 SP 指向生长方向上的下一个空闲单元。空栈没有“最后压入的数据”，初始 SP 由栈边界和所采用的约定确定。

## Cortex-M 的约定

我们常用的 **ARM Cortex-M** 使用的是 **Full Descending Stack**。

架构资料：[Arm《Cortex-M3 Devices Generic User Guide》（DUI 0552A），The Cortex-M3 Processor / Programmers model / Stacks][2]，说明 SP 指向最后压入项，压栈时先递减 SP 再写入。

 [1]: ./images/stack_type.jpg
 [2]: https://support.arm.com/documentation/dui0552/a/the-cortex-m3-processor/programmers-model/stacks
