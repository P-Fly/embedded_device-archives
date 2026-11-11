# 如何生成文本反汇编文件

Keil MDK 使用 Arm 工具链链接生成的可执行映像通常采用 `.axf` 扩展名，其内容是 ELF。具体文件名、输出目录及是否生成可执行映像由目标的 Output 设置决定；扩展名本身不保证文件包含调试信息。`fromelf` 可读取该 AXF/ELF 映像并输出供阅读的反汇编文本。

## fromelf 参数与工具定位

命令形式为 `fromelf [options] input_file`。只需要代码反汇编时，组合 `--text -c`：

| 参数 | 含义与前提 |
| --- | --- |
| `--text` | 选择文本输出模式；具体列出哪些内容由相应选项决定 |
| `-c` | 对代码进行反汇编；不要求存在调试信息，但调试信息和符号是否保留会影响可读性 |
| `-a` | 显示数据地址信息，需要输入映像包含相应调试信息；按需加入，不是输出所有内容的开关 |
| `--output=文件名` | 将文本写入指定文件；输出目录必须已存在且可写 |

使用以下形式生成文本反汇编。工具、输入和输出路径都加引号，允许目录名包含空格：

```bat
"C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe" --text -c --output="C:\Work\Demo Project\Listings\demo.lst" "C:\Work\Demo Project\Objects\demo.axf"
```

## 与 Keil MDK 集成

按原图的界面路径打开 `Project → Options for Target → User → After Build/Rebuild`，勾选 `Run #1`，在 `User Command` 中填写命令。先创建工程下的 `Listings` 目录，再按实际工具路径填写，例如：

```bat
"C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe" --text -c --output="Listings\@L.lst" "#L"
```

这里的 `#L`、`@L` 是 µVision 的 Key Sequence，不是 Windows 环境变量：

| 占位符 | µVision 5.38 手册中的含义 |
| --- | --- |
| `#L` | 链接输出文件的完整路径和文件名，包含扩展名，作为输入映像 |
| `@L` | 链接输出文件名，不含路径和扩展名，作为清单文件名 |

After Build/Rebuild 在构建或重建成功后执行已启用的命令。µVision 5.38 的 `Run 'After-Build' Conditionally` 决定目标已是最新等情况下是否仍运行命令；未勾选时，已启用的 After Build 命令默认仍可执行，勾选时按手册规定的构建或命令/设置变化条件执行。命令诊断进入 Build Output，应检查退出状态、输入映像和输出文件更新时间，不能只凭目标链接成功认定反汇编生成成功。

![User 页中 After Build/Rebuild 的 Run #1 和 User Command 位置][1]

## 参考资料

- [Arm Compiler 5.06 for µVision fromelf User Guide](https://www.keil.com/support/man/docs/armutil/default.htm)：1.3「Getting help」、1.4「Command-line syntax」、3.38「--output」与 3.45「--text」。
- [µVision User's Guide 5.38：User](https://support.arm.com/documentation/101407/0538/Dialogs/Project/Options/User)：User Command、After Build/Rebuild 与条件执行。
- [µVision User's Guide 5.38：Key Sequence for Tool Parameters](https://support.arm.com/documentation/101407/0538/Utilities/Key-Sequence-for-Tool-Parameters)：路径引号、Key Code 与 File Code。

[1]: ./images/fromelf_configure.jpg
