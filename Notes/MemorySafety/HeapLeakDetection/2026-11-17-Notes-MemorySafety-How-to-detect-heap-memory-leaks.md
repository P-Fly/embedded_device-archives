# 如何检测堆内存泄漏

## 适用范围与基本概念

本文介绍动态内存泄漏的常见原因，以及在 FreeRTOS 和 PC 环境下定位、修复和验证的方法。

| 概念 | 含义 | 与泄漏的关系 |
| --- | --- | --- |
| 内存泄漏（Memory Leak） | 对象的预期生命周期已经结束，但分配的内存没有正确释放 | 包括指针丢失，以及仍保留已无业务用途对象的逻辑泄漏 |
| 堆内存耗尽（Heap Exhaustion） | 可用内存无法满足分配请求 | 泄漏、合法占用峰值、容量不足等都可能导致耗尽 |
| 内存碎片 | 空闲空间被分隔，或分配产生对齐等内部损耗 | 分配失败不一定是总空闲量不足，也不一定是泄漏 |
| 堆缓冲区溢出（Heap Buffer Overflow） | 写入超过堆对象的边界 | 属于越界和内存破坏问题 |

**判断泄漏的关键是生命周期和所有权。**

## 常见原因与修复方向

| 原因 | 典型错误 | 修复方向 |
| --- | --- | --- |
| 丢失最后一个有效指针 | 给唯一的指针重新赋值、置空，或离开作用域前没有释放 | 释放旧对象后再覆盖 |
| 正常或失败路径遗漏释放 | 提前返回，部分初始化失败，超时、取消或断开路径漏清理 | 统一清理入口，覆盖部分初始化和所有退出路径 |
| 嵌套对象清理不完整 | 只释放结构体，没有释放其拥有的成员或链表节点 | 按所有权清理 |
| 异步所有权不清楚 | 投递失败无人释放，取消后未回收，消费完成后漏释放 | 明确成功、失败、取消、超时分别由谁持有和释放 |
| 错误使用 `realloc` | 直接执行 `buffer = realloc(buffer, size)`，失败时丢失旧指针 | 使用临时指针并单独处理零长度请求 |
| 引用或容器一直保留对象 | 引用计数不归零、形成环、缓存或列表没有淘汰与退出逻辑 | 核对引用增减和移除路径 |
| 生命周期接口没有成对执行 | 多次创建，遗漏销毁或者任务退出但未清理业务资源 | 明确创建/销毁协议，不假定任务退出会释放任意业务对象 |

## FreeRTOS 环境的排查方法

### 确认真正使用的堆和接口

先核实当前链接的是哪种堆实现、哪些内存域参与分配，以及标准库接口是否映射到了 `pvPortMalloc/vPortFree`。

`heap_4` 类实现通常通过块头保存大小和分配标志，以地址有序的链表管理空闲块，并在释放时合并相邻空闲块。已分配块一般已经从空闲链中摘除。块头、对齐和未被分裂的小余量都会消耗空间。

### 使用现有统计接口观察趋势

| 接口或信息 | 用途 | 注意事项 |
| --- | --- | --- |
| `xPortGetFreeHeapSize()` | 观察当前空闲量 | 只反映对应堆，不代表整个系统的可用内存 |
| `xPortGetMinimumEverFreeHeapSize()` | 观察启动以来的最低空闲量和峰值余量 | 历史最低值不会因释放而回升，不能用它判断当前是否泄漏 |
| `vPortGetHeapStats()` | 若实现提供，可查看最大空闲块、空闲块数及成功分配释放计数 | 可以先验证未初始化、无空闲块及并发等边界行为 |
| 任务、队列和对象统计 | 区分合法存活、消息积压和业务对象遗漏 | 分配次数减释放次数只能辅助判断，不表示差值全是泄漏 |

若总空闲量足够，但最大连续空闲块不足以满足加上块头和对齐后的请求，应考虑内存碎片过大。

对于 FreeRTOS 任务自删除后，其 TCB 和栈需要由 Idle 任务回收。如果 Idle 任务长期得不到运行，可能表现为可用堆持续下降。

### 利用堆快照定位候选对象

优先使用已有诊断接口。若只能读取原始堆，应依据真实布局编写解析器，并满足以下条件：

- 已知堆区域、初始化状态、块头长度、对齐要求、分配标志和全部扩展位。
- 在不会与分配释放并发修改的时机采样。
- 先校验可读范围，再读取块头，之后计算下一块前检查大小、对齐、算术溢出、前进性和终止边界，损坏后停止解析，避免无限循环或二次异常。
- 物理相邻块与空闲链是不同的遍历关系，块大小必须屏蔽所有标志和扩展信息，而不只是最高分配位。

块会分裂、合并和复用，因此块头中残留的释放地址不是完整历史，也不一定对应当前块的单一生命周期。

调试字段会增加占用并改变布局，LR、调用栈、任务占用统计都属于实现相关的扩展，不是所有 FreeRTOS 堆都自带的能力。

### 增加低扰动的连续追踪

快照无法区分的对象，可以在实际分配和释放入口记录事件。下面是插桩位置的伪代码：

```text
分配入口:
	保存原始请求大小和调用点
	调用真实分配逻辑
	成功则记录 ALLOC(地址, 请求大小, 实际块大小, 调用点)
	失败则记录 FAIL(请求大小, 调用点)

释放入口:
	空指针按接口约定处理
	在元数据仍有效时保存地址、大小和调用点
	按明确的事件顺序记录 FREE，并执行真实释放逻辑
```

优先使用预分配的定长记录区，延后在安全上下文导出。不要在分配器临界区同步打印大量日志，也不要让记录器经由日志、锁或字符串格式化再次调用被追踪的分配器。

获取调用点应使用目标编译器支持的机制，并用 `uintptr_t` 等足够宽的类型保存地址，不要假定地址永远是 32 位。

一级返回地址可能只落在公共包装器中，需要调整插桩层次或采集短调用栈。

解析地址时保留同一构建的 ELF、map、固件和加载信息，并使用目标架构工具链的符号化工具，例如：

```sh
addr2line -e firmware.elf -f -C -i 0x08012344
```

## PC 环境的排查方法

PC 上优先使用成熟的泄漏检测工具，手工配对日志作为补充。

### 用同一示例验证泄漏与修复路径

以下完整程序在带 `--leak` 参数运行时故意遗漏释放；不带参数则正确释放。示例仅用于检验诊断方法，文件名约定为 `leak_demo.c`。

```c
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	unsigned char *buffer = malloc(128);
	if (buffer == NULL) {
		return EXIT_FAILURE;
	}
	memset(buffer, 0x5a, 128);
	if (argc > 1 && strcmp(argv[1], "--leak") == 0) {
		return EXIT_SUCCESS;
	}
	free(buffer);
	return EXIT_SUCCESS;
}
```

### LeakSanitizer / AddressSanitizer

在支持 LeakSanitizer 的 Linux 编译器和运行时，可通过 AddressSanitizer 启用泄漏检测，并同时检查部分越界和释放后使用问题：

```sh
gcc -g3 -O0 -fno-omit-frame-pointer -fsanitize=address -Wall -Wextra leak_demo.c -o leak_demo_asan
ASAN_OPTIONS=detect_leaks=1 ./leak_demo_asan --leak
ASAN_OPTIONS=detect_leaks=1 ./leak_demo_asan
```

前一次应报告 128 字节泄漏并以非零状态结束，后一次应正常结束且不报告该泄漏。也可在支持的平台上使用 `-fsanitize=leak` 单独进行泄漏检测。

### Valgrind Memcheck

使用不带 sanitizer 的调试构建运行，避免把两种运行时诊断叠加：

```sh
gcc -g3 -O0 -Wall -Wextra leak_demo.c -o leak_demo
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=definite,indirect --error-exitcode=99 ./leak_demo --leak
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=definite,indirect --error-exitcode=99 ./leak_demo
```

| 分类 | 解读 |
| --- | --- |
| definitely lost | 没有找到指向该块的有效指针，优先排查 |
| indirectly lost | 通常因拥有者对象丢失而连带失去引用，先修复上层拥有者 |
| possibly lost | 只找到内部指针等情况，需要核实对象布局和指针使用 |
| still reachable | 仍能找到引用，可能是合理常驻对象，也可能是逻辑泄漏 |

### 检测边界

- LSan 默认在进程正常退出时执行泄漏检查。对于长时间运行的程序，可在明确的业务检查点使用工具支持的显式检查接口，不能仅凭尚未出现退出报告就判断没有泄漏。
- LSan 和 Memcheck 的泄漏判定依赖对象是否能从根集合到达。全局容器、缓存等仍持有引用的对象，即使已无业务用途，也可能不被 LSan 报告，或被 Memcheck 归为 `still reachable`。是否属于逻辑泄漏，仍需结合预期生命周期和所有权判断。
- 自定义内存池中的子分配不一定对工具可见。若工具只识别内存池申请的整块内存，就无法自动判断池内每个对象是否遗漏释放，需要为池接口增加追踪，或使用工具提供的自定义分配器标注接口。

### 通过链接包装补充分配日志

当工具无法覆盖目标环境，或需要把分配与业务事件关联时，可以使用分配入口插桩。GNU 风格链接器的 `--wrap=symbol` 会把对指定符号的未定义引用重定向到 `__wrap_symbol`，并把 `__real_symbol` 引用解析到原符号。

下面是可编译的单线程演示，文件名约定为 `wrap_demo.c`。

```c
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define EVENT_CAPACITY 32

typedef struct {
	char operation;
	uintptr_t address;
	size_t requested_size;
	uintptr_t caller;
} HeapEvent;

static HeapEvent events[EVENT_CAPACITY];
static size_t event_count;
static int recording_enabled;
static int events_lost;

extern void *__real_malloc(size_t size);
extern void __real_free(void *address);

static void record_event(char operation, const void *address,
                         size_t requested_size, void *caller)
{
	if (!recording_enabled) {
		return;
	}
	if (event_count == EVENT_CAPACITY) {
		events_lost = 1;
		return;
	}
	events[event_count++] = (HeapEvent){
		operation, (uintptr_t)address, requested_size, (uintptr_t)caller
	};
}

void *__wrap_malloc(size_t size)
{
	void *address = __real_malloc(size);
	void *caller = __builtin_extract_return_addr(__builtin_return_address(0));
	record_event(address != NULL ? 'A' : 'X', address, size, caller);
	return address;
}

void __wrap_free(void *address)
{
	if (address != NULL) {
		void *caller = __builtin_extract_return_addr(__builtin_return_address(0));
		record_event('F', address, 0, caller);
	}
	__real_free(address);
}

int main(void)
{
	recording_enabled = 1;
	void *buffer = malloc(64);
	int allocation_failed = (buffer == NULL);
	free(buffer);
	recording_enabled = 0;

	for (size_t index = 0; index < event_count; ++index) {
		printf("%zu %c ptr=0x%" PRIxPTR " requested=%zu caller=0x%" PRIxPTR "\n",
		       index + 1, events[index].operation, events[index].address,
		       events[index].requested_size, events[index].caller);
	}
	printf("events_lost=%d\n", events_lost);
	return allocation_failed || events_lost ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

```sh
gcc -g3 -O0 -fno-builtin-malloc -fno-builtin-free -Wall -Wextra wrap_demo.c -Wl,--wrap=malloc -Wl,--wrap=free -o wrap_demo
./wrap_demo
```

正常情况下输出同一地址的一次 `A`（成功分配）和一次 `F`（释放），且 `events_lost=0`；`X` 表示分配失败，释放记录的大小应回查对应的分配记录。

## 参考资料

- [FreeRTOS-Kernel V10.5.1 heap_4.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V10.5.1/portable/MemMang/heap_4.c)：块头布局、空闲链表、分裂与合并，以及堆统计接口的实现。
- [FreeRTOS vTaskDelete](https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/03-vTaskDelete)：任务删除、Idle 延迟回收，以及业务代码分配内存的清理责任。
- [GCC Instrumentation Options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)：`-fsanitize=address`、`-fsanitize=leak` 等编译选项及运行时设置。
- [Clang LeakSanitizer](https://clang.llvm.org/docs/LeakSanitizer.html)：LSan 的使用方式、支持平台、显式检查接口和检测时机。
- [Valgrind Memcheck Manual](https://valgrind.org/docs/manual/mc-manual.html)：4.2.10「Memory leak detection」、4.3「Memcheck Command-Line Options」和 4.8「Memory Pools: describing and working with custom allocators」。
- [GNU ld Command-line Options](https://sourceware.org/binutils/docs/ld/Options.html)：`--wrap=symbol`、`__wrap_symbol`、`__real_symbol` 的解析规则及适用范围。
