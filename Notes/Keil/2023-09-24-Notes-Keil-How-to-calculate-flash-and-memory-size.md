# 如何估算 Flash 和 RAM 的占用

Keil MDK 使用 Arm 链接器构建后，Build Output 中的 `Program Size` 给出映像的分类统计，单位为 B。

```text
linking...
Program Size: Code=14348 RO-data=676 RW-data=284 ZI-data=36416
FromELF: creating hex file...
```

| 分类 | 含义 | 常见存放方式 |
| --- | --- | --- |
| `Code` | 链接后保留的机器指令 | 在 Flash 中加载并执行 |
| `RO-data` | Read-only data，只读数据 | 在 Flash 中加载并访问 |
| `RW-data` | Read-write data，带初值的可读写数据 | 初值存于 Flash，运行时数据位于 RAM |
| `ZI-data` | Zero-initialized data，零初始化数据 | 在 RAM 中分配，启动时清零，不需要同等大小的 Flash 初值副本 |

## 常见布局下的估算

若代码和只读数据在 Flash 中执行或访问，RW 初值从 Flash 复制到 RAM，ZI 在 RAM 中清零，则可估算：

$$
S_{\mathrm{Flash,estimate}} = S_{\mathrm{Code}} + S_{\mathrm{RO}} + S_{\mathrm{RW}}
$$

$$
S_{\mathrm{RAM,estimate}} = S_{\mathrm{RW}} + S_{\mathrm{ZI}}
$$

这组输出对应 Flash 映像估算 `14348 + 676 + 284 = 15308 B`，RAM 执行空间估算 `284 + 36416 = 36700 B`。

## 加载视图与执行视图

加载视图（Load View）描述映像在启动前的存储位置，执行视图（Execution View）描述运行时的位置。RW 的初值副本占加载区空间，初始化后可修改的数据占执行区空间，因此通常同时计入 Flash 和 RAM。ZI 只需在执行区预留空间，由启动初始化代码清零。

![RW 从 ROM 加载区复制到 RAM 执行区，ZI 在 RAM 中清零][1]

## 用 map 核对资源预算

以实际 scatter 文件和链接器 map 为准。启用 armlink 的 `--map`，结合 `--info=sizes` 查看分类统计；在 MDK 中也可通过链接器的映像映射选项生成报告。逐个对照 Load Region 与 Execution Region 的基址、大小、上限及 section 分配，确认它们实际落在内部 Flash、内部 RAM 还是外部存储器中。

| 核对项 | 预算处理 |
| --- | --- |
| 栈与堆 | 检查 map 中的 `STACK`、`HEAP`、ZI section 或 `EMPTY` 预留区。若空间已包含在 ZI/执行区大小中，不再重复相加；若只由运行时约定预留而未纳入统计，则另列预算。动态分配的任务栈来自已有堆时，也不能将堆容量与任务栈再次全额相加 |
| `NOINIT`、`UNINIT` | 未初始化数据仍占执行存储空间，但不需要初值副本，也不应按普通 ZI 假定启动时清零。源码段名与 scatter 的 `UNINIT` 属性须结合 map 和启动代码核对 |
| 对齐与 padding | section/region 对齐、链接器填充、地址空洞以及 Flash 擦除块预留可能影响区域跨度或可用预算，不能只按分类字节数判断是否放得下 |
| RW 压缩 | 压缩后的加载数据大小不再等于 RW 的执行大小，还需考虑解压代码和相关表；RAM 仍按解压后的执行布局计算 |
| 多加载区与多执行区 | 各个区域分别检查容量和重叠；代码在 RAM 执行、XO 区域、外部 Flash/RAM 等布局不能直接套用上述两个总和 |
| 映像之外的对象 | bootloader、其他固件映像、参数区、文件系统、升级槽及保留区须单独核对；外部存储占用不等于片内占用 |

## 参考资料

参考版本为官方 **Arm Compiler 5.06 for µVision** 的 [armlink User Guide](https://www.keil.com/support/man/docs/armlink/default.htm)，重点为 3.1.3「Load view and execution view of an image」、4.7「Optimization with RW data compression」、8「Scatter File Syntax」、9.53「--info」和 9.73「--map」。

[1]: ./images/load_view_and_execution_view.jpg
