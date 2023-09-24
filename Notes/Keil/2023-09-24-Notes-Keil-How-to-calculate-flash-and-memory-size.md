# 如何计算Flash和RAM的大小

在 **keil MDK** 编译后，下方会显示编译情况，这其中就包含了 **FLASH** 和 **RAM** 的信息：

```
linking...
Program Size: Code=14348 RO-data=676 RW-data=284 ZI-data=36416
FromELF: creating hex file...
```

 - **Code**：编译器生成的机器码所占用的大小。

 - **RO-data**：编译器生成的只读数据区所占用的大小，**Read Only-Data**。

 - **RW-data**：编译器生成的读写数据区所占用的大小，**Read Write-Data**。

 - **ZI-data**： 编译器生成的 **0数据区** 所占用的大小，一般由启动代码初始化为 **0**，**Zero Initialie-Data**。

**FLASH** 和 **RAM** 的计算方式如下：

 - **FLASH = Code + RO-data + RW-data** = 14348 + 676 + 284 = 15308Byte

 - **RAM = RW-data + ZI-data** = 284 + 36416 = 36700Byte

这里面容易疑惑的是 **RW-data** 为什么在 **FLASH** 和 **RAM** 中都需要计算，这就牵涉 **Load View** 和 **Execution View** 这两个视图。下图通过 **RW section** 的搬移说明了这个问题。

![Load View And Execution View][1]

 [1]: ./images/load_view_and_execution_view.jpg
