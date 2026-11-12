# Stack Canary 机制

## 保护目标

Stack canary 是编译器插入的函数栈帧完整性检查。受保护函数在入口将参考 guard 保存到自己的栈帧，正常退出前比较栈内副本与参考值；不一致时调用 `__stack_chk_fail()`，而不是继续执行正常返回路径。

它主要检测局部缓冲区越界等写操作对栈帧的破坏，不是线程栈容量检查。递归过深、大型自动变量或异常嵌套导致的栈空间耗尽，需要另行配置栈限、guard page 或运行时边界检查。

## 可复现实验

### 环境

以下示例在 WSL2 的 x86-64 Linux 上验证：

| 项目 | 验证环境 |
| --- | --- |
| GCC | Ubuntu GCC 11.4.0 |
| 编译目标 | `x86_64-linux-gnu` |
| C 运行库 | glibc 2.35 |
| 命令解释器 | Bash |

### 示例文件

- [canary_demo.c](demo/canary_demo.c)：唯一的越界示例。
- [Makefile](demo/Makefile)：构建、预期失败验证、反汇编和清理入口。
- [README.md](demo/README.md)：环境要求、运行方法及预期结果。

示例在 `copy_text()` 中使用 `strcpy()` 向 16 字节的局部数组 `buffer` 拷贝输入。实验参数为 31 个 `A`，连同结尾的 NUL 共写入 32 字节。在本次栈布局下，这次写入覆盖 canary，但尚未覆盖保存的帧指针和返回地址。

### 编译与检测结果

参数选择：

- `-fstack-protector-strong`：为具有局部数组等特征的函数插入 canary 检查。
- `-O0 -fno-omit-frame-pointer`：便于将机器代码与源码对应，并用 `rbp` 表达本次栈布局；生产构建不要求使用这些参数。
- `_FORTIFY_SOURCE=0`：排除 fortified libc 拷贝检查先于 canary 报错的干扰；本实验也不启用 AddressSanitizer。
- 验证目标在独立 shell 中执行 `ulimit -c 0`，将 core 文件大小限制设为 0，不改变调用终端的限制。若 `kernel.core_pattern` 使用管道式崩溃收集器，内核不强制该限制，因此不能保证禁止收集或转储。

观察到的关键输出为：

```text
copy finished
*** stack smashing detected ***: terminated
exit status: 134
```

Bash 可能额外打印 `Aborted`。glibc 的失败处理以 `SIGABRT` 终止进程，Bash 通常将退出状态表示为 `128 + 6 = 134`。

`copy finished` 出现而 `returned to main` 未出现，说明越界的拷贝已经执行完成，随后在 `copy_text()` 的退出检查中终止。Canary 不会在 `strcpy()` 写入第 17 字节时立即阻止写操作。

### 反汇编与栈布局

示例的反汇编目标输出 `copy_text()` 的实际机器代码。本次产物中的检查路径为：

| 阶段 | 关键操作 |
| --- | --- |
| 函数入口 | 保存 `rbp`，分配 `0x30` 字节栈帧。 |
| 保存 guard | 从 `fs:0x28` 读取参考值，存入 `[rbp-0x8]`。 |
| 执行拷贝 | 将位于 `rbp-0x20` 的数组地址传给 `strcpy()`。 |
| 退出检查 | 读取 `[rbp-0x8]`，减去 `fs:0x28` 的参考值。 |
| 分支处理 | 相等则恢复栈帧并返回，否则调用 `__stack_chk_fail()`。 |

这里的 `fs:0x28` 是本次 x86-64 glibc 环境中的线程本地 guard 位置；`[rbp-0x8]` 是本次函数栈帧内的副本。

```text
高地址
rbp + 0x08 .. rbp + 0x0f   返回地址                 8 B
rbp + 0x00 .. rbp + 0x07   保存的 rbp              8 B
rbp - 0x08 .. rbp - 0x01   canary                  8 B
rbp - 0x10 .. rbp - 0x09   padding                 8 B
rbp - 0x20 .. rbp - 0x11   buffer[16]             16 B
rbp - 0x28 .. rbp - 0x21   text 参数的栈内副本     8 B
rbp - 0x30 .. rbp - 0x29   对齐空间                8 B
低地址                     rsp = rbp - 0x30
```

栈帧分配使 `rsp` 向低地址移动，但数组按递增索引向高地址写入。此次 32 字节拷贝从 `rbp-0x20` 写到 `rbp-0x01`，依次经过数组、padding 和 canary。

## GCC 插桩范围

以下选项控制哪些函数被纳入检查，并不是每一种选项都保护全部函数：

| 选项 | 主要选择规则 |
| --- | --- |
| `-fstack-protector` | 含 `alloca()`，或具有达到阈值的局部缓冲区等易受影响对象；GCC 默认缓冲区阈值为 8 字节，可由 `--param ssp-buffer-size` 调整。 |
| `-fstack-protector-strong` | 扩展至具有局部数组，或引用局部栈帧地址的函数。 |
| `-fstack-protector-all` | 对所有函数请求插入检查，而非按上述特征选择。 |
| `-fstack-protector-explicit` | 仅保护带 `stack_protect` 属性的函数。 |

`-fstack-protector` 和 `-fstack-protector-strong` 按局部对象选择函数时，只考虑实际分配到栈上的变量；被优化消除或分配到寄存器中的变量不计入。因此，源码中存在局部数组并不必然意味着产物中有 canary 检查。

## 能力边界

- 检测发生在编译器插入的检查点，通常是正常退出前。此前可能已破坏其他局部数据、全局对象或产生外部副作用。
- 写操作必须改变被检查的 canary 才会报警。未触及它、跨过它直接写入其他位置、写入相同值，或恢复原值，都可能不被发现。
- 不检查堆缓冲区、全局缓冲区和一般越界读；也不保证覆盖函数指针等所有控制数据。
- 运行库应使用不可预测的 guard；固定常量可以发现偶发破坏，但不适合作为对抗已知值攻击者的唯一防线。随机 guard 也不能替代边界检查。
- Canary、内存访问权限保护和线程栈限解决不同问题，部署其中一种并不意味着其余风险已经消失。
