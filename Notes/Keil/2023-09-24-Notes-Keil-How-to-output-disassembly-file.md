# 如何生成反汇编文件

## 前言

在 **keil MDK** 的集成开发环境中，默认情况下是生成 **.axf** 格式的调试文件。但有些问题需要我们通过汇编代码进行分析。此时，我们需要使用 **fromelf** 工具对生成的 **.axf** 文件进行反汇编。

## fromelf

**fromelf** 的命令格式如下：

> fromelf options input_file

可以使用 **--text** 参数指定反汇编的输出格式：

> fromelf --text -a -c --output=xxx\xxx.asm xxx\xxx.axf

## 与 keil 集成

按照下列路径打开窗口，并添加命令：

**Project -> Options for target -> User -> After Build/Rebuild**

![fromelf configure][1]

 [1]: ./images/fromelf_configure.jpg
