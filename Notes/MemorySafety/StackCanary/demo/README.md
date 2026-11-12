# Stack Canary Demo

唯一示例故意使用 `strcpy()` 将 31 个 `A` 及结尾 NUL 写入 16 字节局部数组，在已验证的栈布局下覆盖 canary，并在函数退出检查中触发 `__stack_chk_fail()`。

## 构建与验证

在本目录运行：

```bash
make
make check
```

`make check` 在独立 shell 中执行 `ulimit -c 0`，将 core 文件大小限制设为 0，不改变调用终端的限制。若 `kernel.core_pattern` 使用管道式崩溃收集器，内核不强制该限制，因此不能保证禁止收集或转储。

随后使用 31 个 `A` 运行程序，并验证以下条件：

- 程序输出 `copy finished`。
- glibc 输出 `stack smashing detected`。
- 程序因 `SIGABRT` 终止，Bash 取得退出状态 `134`。
- 不出现 `returned to main`。

关键输出：

```text
copy finished
*** stack smashing detected ***: terminated
exit status: 134
PASS: canary failure detected before returning to main.
```

Bash 可能额外打印 `Aborted`。预期的程序终止被验证目标视为成功，因此 `make check` 在上述条件全部满足时返回 `0`，否则失败。`make run` 是同一验证流程的别名，不增加其他示例。

## 反汇编

```bash
make disasm
```

目标使用 Intel 语法输出 `copy_text()` 的反汇编。

## 清理

```bash
make clean
```
