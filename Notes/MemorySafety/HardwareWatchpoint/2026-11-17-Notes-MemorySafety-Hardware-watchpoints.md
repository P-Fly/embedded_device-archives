# 利用硬件 Watchpoint 定位内存破坏

## 适用范围与工作机制

本文面向 Cortex-M 的内存破坏定位，寄存器配置示例采用 Armv8-M Mainline DWT 的普通数据地址匹配方式。

硬件 watchpoint 利用 Data Watchpoint and Trace (DWT) 单元比较本核的数据访问地址。在指定的读、写或读写访问命中后，DWT 可以生成调试事件，再由调试配置决定进入 Debug state，还是触发 DebugMonitor 异常。写事件命中时，数据可能已经被改写。

需要区分三个层次：比较器是否匹配、匹配后生成什么动作，以及处理器如何处理该动作。比较器匹配不意味着在所有执行上下文中都能立即停止或进入异常。

## 关键寄存器与匹配语义

### 调试事件的处理路径

| 配置 | 作用 |
| --- | --- |
| `DEMCR.TRCENA` | 使能 DWT、ITM 等相关功能 |
| `DHCSR.C_DEBUGEN` | 使能 halting debug。置位时，符合条件的调试事件可使处理器进入 Debug state |
| `DEMCR.MON_EN` | 在未启用 halting debug 时，使能相应调试事件的 DebugMonitor 处理路径 |
| `DWT_FUNCTIONn.ACTION=1` | 匹配时生成 debug event，不直接选择 Halt 或 Monitor |

`C_DEBUGEN` 由外部调试访问控制，不能依靠普通目标程序写寄存器来切换。连接调试器、调试器退出和不同复位方式都可能影响调试状态，不能用“是否插着调试器”替代寄存器判断。

DebugMonitor 路径要求事先安装异常处理入口并设置合适的异常优先级。执行优先级、异常屏蔽状态和安全调试授权都会影响事件处理，因此在高优先级 ISR、临界区、NMI 或 HardFault 中发生的访问，可能不会立即进入 DebugMonitor。

![DEMCR 寄存器][1]

### 比较器配置

`DWT_CTRL.NUMCOMP` 表示实现的比较器数量。若数量为 `num`，有效索引是 `0` 到 `num - 1`；`num=0` 表示没有可用比较器。

Armv8-M 普通数据地址匹配的常用字段如下：

| 字段 | 编码 | 含义 |
| --- | --- | --- |
| `MATCH` | `0` | 禁用比较器 |
| `MATCH` | `4` | 数据地址读写匹配 |
| `MATCH` | `5` | 数据地址只写匹配 |
| `MATCH` | `6` | 数据地址只读匹配 |
| `DATAVSIZE` | `0` | 1 字节对象 |
| `DATAVSIZE` | `1` | 2 字节对象 |
| `DATAVSIZE` | `2` | 4 字节对象 |
| `ACTION` | `1` | 生成 debug event |

`MATCH=7` 属于数据地址范围上限匹配，不能当作普通读写匹配使用。`DATAVSIZE` 是对象大小的编码。普通地址匹配应使用与对象大小对应的自然对齐地址。

![DWT_FUNCTION 寄存器][2]

## 配置示例

接口返回 `0` 表示成功，`-1` 表示参数错误，`-2` 表示调试模式不符或 DWT 未使能，`-3` 表示没有比较器或配置无法读回。

### 初始化与单通道配置

```c
#include <stddef.h>
#include <stdint.h>
#include "cmsis.h"

enum watchpoint_access {
    WP_READ_WRITE = 4,
    WP_WRITE = 5,
    WP_READ = 6
};

enum watchpoint_delivery {
    WP_HALT,
    WP_MONITOR
};

struct watchpoint_registers {
    uint32_t comp;
    uint32_t reserved0;
    uint32_t function;
    uint32_t reserved1;
};

_Static_assert(sizeof(uintptr_t) == sizeof(uint32_t), "32-bit address required");
_Static_assert(sizeof(struct watchpoint_registers) == 0x10U,
               "DWT comparator stride must be 16 bytes");
_Static_assert(offsetof(struct watchpoint_registers, function) == 0x08U,
               "Invalid DWT FUNCTION offset");

static uint32_t watchpoint_count(void)
{
    return (DWT->CTRL & DWT_CTRL_NUMCOMP_Msk) >> DWT_CTRL_NUMCOMP_Pos;
}

static volatile struct watchpoint_registers *watchpoint_regs(uint32_t index)
{
    uintptr_t base = (uintptr_t)&DWT->COMP0;

    return (volatile struct watchpoint_registers *)
        (base + (uintptr_t)index * sizeof(struct watchpoint_registers));
}

int watchpoint_debug_init(enum watchpoint_delivery delivery)
{
    uint32_t halting = CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
    uint32_t enable_bits = CoreDebug_DEMCR_TRCENA_Msk;

    if (delivery != WP_HALT && delivery != WP_MONITOR) {
        return -1;
    }
    if ((delivery == WP_HALT && halting == 0U) ||
        (delivery == WP_MONITOR && halting != 0U)) {
        return -2;
    }
    if (delivery == WP_MONITOR) {
        enable_bits |= CoreDebug_DEMCR_MON_EN_Msk;
    }

    CoreDebug->DEMCR |= enable_bits;
    __DSB();
    __ISB();

    if ((CoreDebug->DEMCR & enable_bits) != enable_bits ||
        watchpoint_count() == 0U) {
        return -3;
    }
    return 0;
}

int watchpoint_disable(uint32_t index)
{
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U) {
        return -2;
    }
    if (index >= watchpoint_count()) {
        return -1;
    }

    watchpoint_regs(index)->function = 0U;
    __DSB();
    __ISB();
    return 0;
}

int watchpoint_set(uint32_t index, const volatile void *address,
                   size_t size, enum watchpoint_access access)
{
    uintptr_t watched_address = (uintptr_t)address;
    uint32_t size_encoding;
    uint32_t function;
    uint32_t function_mask;
    volatile struct watchpoint_registers *registers;

    if (access != WP_READ && access != WP_WRITE && access != WP_READ_WRITE) {
        return -1;
    }
    switch (size) {
    case 1U:
        size_encoding = 0U;
        break;
    case 2U:
        size_encoding = 1U;
        break;
    case 4U:
        size_encoding = 2U;
        break;
    default:
        return -1;
    }
    if ((watched_address & (uintptr_t)(size - 1U)) != 0U) {
        return -1;
    }
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U) {
        return -2;
    }
    if (index >= watchpoint_count()) {
        return -1;
    }

    function = (size_encoding << DWT_FUNCTION_DATAVSIZE_Pos) |
               (1UL << DWT_FUNCTION_ACTION_Pos) | (uint32_t)access;
    function_mask = DWT_FUNCTION_DATAVSIZE_Msk |
                    (3UL << DWT_FUNCTION_ACTION_Pos) | DWT_FUNCTION_MATCH_Msk;
    registers = watchpoint_regs(index);
    registers->function = 0U;
    __DSB();
    registers->comp = (uint32_t)watched_address;
    registers->function = function;
    __DSB();
    __ISB();

    if ((registers->function & function_mask) != function ||
        registers->comp != (uint32_t)watched_address) {
        (void)watchpoint_disable(index);
        return -3;
    }
    return 0;
}
```

初始化按位补齐 `TRCENA` 和所需的 `MON_EN`，不能用 `!(DEMCR & (TRCENA | MON_EN))` 判断是否需要使能；后者在已有其中一位时会漏判。Halt 路径不需要通过清除 `MON_EN` 来选择模式，已有全局调试配置应由其所有者管理。

重配顺序为禁用比较器、写入地址、最后写入匹配功能。

### 最小写访问示例

示例使用已分配给调用方的通道 0，选择 DebugMonitor 路径。预期观察对象是写访问，不依赖写前写后的数值是否相同。`volatile` 保留实际的测试存储操作，不承担并发同步职责。

```c
static volatile uint32_t watched_word;

int watchpoint_test(void)
{
    int status = watchpoint_debug_init(WP_MONITOR);

    if (status != 0) {
        return status;
    }
    status = watchpoint_disable(0U);
    if (status != 0) {
        return status;
    }

    watched_word = 0x19U;
    status = watchpoint_set(0U, &watched_word, sizeof(watched_word), WP_WRITE);
    if (status != 0) {
        return status;
    }
    watched_word = 0x55U;

    return watchpoint_disable(0U);
}
```

若由外部调试器接收事件，初始化改用 `WP_HALT`，并确认调试器没有覆盖该通道。该函数没有安装异常处理器；命中后的停止、保留现场或返回策略属于下一节的异常处理流程。

## 命中后的现场分析

### 保留原始状态

Halt 路径直接使用调试器寄存器和内存视图；DebugMonitor 路径应在处理入口尽早保留异常现场和调试状态，不依赖某种平台日志格式。

1. 保存异常进入时的 `EXC_RETURN`、通用寄存器及对应栈帧，依据 `EXC_RETURN` 区分 MSP/PSP、基本帧/浮点扩展帧；带安全扩展时同时区分安全状态。
2. 快照 `SCB->DFSR`，用 `DWTTRAP` 判断是否有 DWT 调试事件，同时保留其他事件位。记录 `DHCSR`、`DEMCR` 和相关比较器的 `COMPn`、`FUNCTIONn`，由 `MATCHED` 辅助定位通道。`DFSR` 是粘滞状态，不代表每个位都来自本次事件；`MATCHED` 具有读清除语义，避免调试器自动刷新或重复读取破坏证据。
3. 保留被观察对象、邻接内存和必要的任务/中断上下文。若读取对象本身会命中读观察点，先保存比较器状态，再禁用相应通道，避免采集过程引入新事件。
4. 若处理后继续运行，对已处理的 `DFSR` 状态按写 1 清除规则处理，并明确是否重新启用比较器。若选择停止或进入故障处理路径，应保证现场不会被后续异常和复位覆盖。

DebugMonitor 并非普通可重入回调。处理路径应避免依赖调度器、动态分配或可能被打断代码持有的锁；栈破坏场景还要考虑异常压栈和处理器自身栈空间是否可靠。

### 从访问现场定位指令

围绕现场 PC 查看指令流，结合寄存器、寻址偏移、访问宽度及写入值，重建实际有效地址，并确认它与观察对象重叠。重点检查 `STR/STRB/STRH`、多寄存器存储、函数内联以及 `memcpy/memset` 等展开代码。

命中表明发生了所选访问，不自动证明这是错误访问。若命中的是合法初始化或更新，继续缩小启用时机、访问类型和对象生命周期；若目标值已经损坏才启用，则应将布点提前。

## RTOS 栈边界监控

以 FreeRTOS 等 RTOS 为例，可以复用一个比较器，随上下文切换监控即将运行任务的栈边界。

对于向低地址增长的栈，通常选择分配区低地址端预留的 guard word。该区域必须排除在任务正常可用栈空间之外，避免把合法满栈写入当作越界；向高地址增长的栈则选择相反边界。任务栈 PSP 与中断栈 MSP 应分别考虑。

在调度器已选定新任务、恢复其执行之前，获取新任务 guard 地址并调用下面的适配函数。首次任务启动也需配置。调试模式初始化在调度启动前完成，通道由调度器独占，失败返回值必须由移植层处理。

```c
int watchpoint_on_task_switch(uint32_t index, const volatile void *guard_address)
{
    return watchpoint_set(index, guard_address, sizeof(uint32_t), WP_WRITE);
}
```

这里监控的是一个对齐的 4 字节对象。若要监控两个 word，可分配两个比较器分别观察，或使用目标实现支持的地址范围匹配。

这种方法存在明确的覆盖边界：

- 只有命中 guard 区域的访问才会触发。一次大栈帧分配直接跨过 guard，随后只访问边界外地址，可能漏检；仅移动 SP 而未产生相关访问也不会命中。
- 一个通道随任务切换，只覆盖当前被配置的任务栈，不能同时监控所有休眠任务。跨任务指针误写其他任务栈可能不在覆盖范围内。
- 命中来自本核的任何符合条件访问，包括 ISR，并不必然来自 guard 所属任务。
- 它提供访问诊断而非越界阻断。

## Watchpoint 使用边界

### 本核可见性与系统影响

- 本核 DWT 不是系统总线监视器，通常不能捕获 DMA、外设或其他核造成的内存修改。多核系统需在实际发起错误访问的核布点，或使用适合总线级访问的诊断机制。
- 地址匹配针对处理器的访问地址。共享内存、安全/非安全别名和其他映射可能指向同一存储，却使用不同地址；不要仅凭物理存储相同推断一定命中。
- 调试器和固件共用有限的比较器。连接、恢复、单步等操作可能重写配置，应明确通道所有权；复位、掉电或低功耗切换后的保留状态也由具体实现决定。
- 比较器匹配本身无需逐条软件检查，但 Halt、异常处理、条件过滤和任务切换重配都会改变时序。停止本核不意味着 DMA、外设或其他核同时停止，要考虑看门狗、协议超时和共享状态变化。

### 保持地址与构建对应

增加定位代码可能改变代码布局、数据布局、栈用量和时序。确需保持特定布局时，应结合链接脚本、预留空间及前后 map 对比。

## 参考资料

- [Armv8-M Architecture Reference Manual](https://support.arm.com/documentation/ddi0553/latest/)
- [Arm Cortex-M33 Processor Technical Reference Manual](https://support.arm.com/documentation/100230/0100/)
- [CMSIS-Core](https://arm-software.github.io/CMSIS_6/latest/Core/index.html)
- [OpenOCD Cortex-M watchpoint 配置实现](https://openocd.org/doc/doxygen/html/cortex__m_8c_source.html#l02146)

[1]: ./images/DEMCR.JPG
[2]: ./images/DWT_FUNCTION.JPG
