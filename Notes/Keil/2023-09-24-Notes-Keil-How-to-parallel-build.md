# 如何配置并行构建

Keil MDK 的 µVision 支持并行构建，可同时处理多个可独立编译的源文件。这里的设置控制主机上的构建工作，不控制程序运行时并发，也不涉及向多块设备并行烧录。

## 配置入口与选项

打开 `Edit → Configuration → Other`，在 `Parallel Build Configuration` 区域设置以下选项。

| 选项 | 含义 |
| --- | --- |
| `Disable Parallel Build` | 勾选时禁用并行构建；5.38 手册说明此时构建过程使用一个线程。取消勾选后才应用下面的 jobs 设置 |
| `Number of parallel jobs` | 配置并行构建作业数，用于控制并发编译工作量；5.38 手册以使用的 CPU 核心数量描述该设置。它不是整个 IDE、编译器及其他工具的固定总线程数承诺，也不保证这些核心始终满载 |

## 参考资料

[µVision User's Guide 5.38：Other](https://support.arm.com/documentation/101407/0538/Dialogs/Edit/Configuration/Other) 的「Parallel Build Configuration」说明了配置入口、禁用状态与 jobs 设置.
