# 如何生成文本反汇编文件

Keil MDK 使用 Arm 工具链链接生成的可执行映像通常采用 `.axf` 扩展名，其内容是 ELF。`fromelf` 可读取该 AXF/ELF 映像并输出供阅读的反汇编文本。

## fromelf 参数与工具定位

命令形式为 `fromelf [options] input_file`。只需要代码反汇编时，组合 `--text -c`：

| 参数 | 含义与前提 |
| --- | --- |
| `--text` | 选择文本输出模式 |
| `-c` | 对代码进行反汇编 |
| `-a` | 显示数据地址信息，需要输入映像包含相应调试信息 |
| `--output=文件名` | 将文本写入指定文件 |

使用以下形式生成文本反汇编：

```bat
"C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe" --text -c --output="C:\Work\Demo Project\Listings\demo.lst" "C:\Work\Demo Project\Objects\demo.axf"
```

## 与 Keil MDK 集成

按原图的界面路径打开 `Project → Options for Target → User → After Build/Rebuild`，勾选 `Run #1`，在 `User Command` 中填写命令：

```bat
"C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe" --text -c --output="Listings\@L.lst" "#L"
```

这里的 `#L`、`@L` 是 µVision 的 Key Sequence：

| 占位符 | µVision 5.38 手册中的含义 |
| --- | --- |
| `#L` | 链接输出文件的完整路径和文件名，包含扩展名，作为输入映像 |
| `@L` | 链接输出文件名，不含路径和扩展名，作为清单文件名 |

After Build/Rebuild 在构建或重建成功后执行已启用的命令。

![User 页中 After Build/Rebuild 的 Run #1 和 User Command 位置][1]

## 参考资料

- [Arm Compiler 5.06 for µVision fromelf User Guide](https://www.keil.com/support/man/docs/armutil/default.htm)：1.3「Getting help」、1.4「Command-line syntax」、3.38「--output」与 3.45「--text」。
- [µVision User's Guide 5.38：User](https://support.arm.com/documentation/101407/0538/Dialogs/Project/Options/User)：User Command、After Build/Rebuild 与条件执行。
- [µVision User's Guide 5.38：Key Sequence for Tool Parameters](https://support.arm.com/documentation/101407/0538/Utilities/Key-Sequence-for-Tool-Parameters)：路径引号、Key Code 与 File Code。

[1]: ./images/fromelf_configure.jpg
