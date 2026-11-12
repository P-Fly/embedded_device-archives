# 如何配置并行构建

Keil MDK 的 µVision 支持并行构建，可同时处理多个可独立编译的源文件。

## 配置入口与选项

打开 `Edit → Configuration → Other`，在 `Parallel Build Configuration` 区域设置以下选项。

| 选项 | 含义 |
| --- | --- |
| `Disable Parallel Build` | 勾选时禁用并行构建 |
| `Number of parallel jobs` | 配置并行构建作业数，用于控制并发编译工作量 |

## 参考资料

[µVision User's Guide 5.38：Other](https://support.arm.com/documentation/101407/0538/Dialogs/Edit/Configuration/Other) 的「Parallel Build Configuration」说明了配置入口、禁用状态与 jobs 设置.
