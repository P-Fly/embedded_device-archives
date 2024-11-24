# 如何压缩日志以减少数据传输

## 问题背景

在嵌入式系统开发中，日志系统是不可或缺的一部分。它可以帮助我们监控系统状态，提供详尽的运行信息，极大提升项目的开发与维护的效率。

正是因为日志信息的重要性，而一个项目可能需要输出的日志又非常多，因此在日志的串口输出过程中，很有可能会造成日志丢失的现象。

因此我们在项目中可以通过对日志进行压缩的方式进行输出，以减少串口的输出负载。

## 方案简介

 ![Introduction][1]

我们的压缩方案分为三个部分：

 1. 在 **编译** 时，将代码的 **格式字符串** 记录到单独的文件中。

 2. 在 **系统运行** 时，只需要输出 **格式字符串的ID** 和 **可变参数的值**。

 3. 在 **收到日志信息** 后，根据编译时的文件，重新恢复完整的日志信息。

比如下面的代码中，**格式字符串** 为 **"This is test 2, ch = %c.\n"**，**可变参数** 为 **ch**。

```
    char ch = 'A';

    ...

    printf("This is test 2, ch = %c.\n", ch);
```

## 详细方案

### 分离格式字符串

在链接脚本文件中，我们增加一个叫做 **.log_string** 的段，这个段中会存储日志系统的所有 **格式字符串**。

```
  .log_string     :
  {
    PROVIDE_HIDDEN (__log_string_start = .);
    KEEP (*(.log_string .log_string.*))
    PROVIDE_HIDDEN (__log_string_end = .);
  }
```

在源码中，我们封装了新的日志打印宏 **LOG_MSGID_I**，该宏会将 **格式字符串** 放到特定的段中。

```
#define ATTR_LOG_STRING __attribute__ ((__section__(".log_string"))) static const char

#define LOG_MSGID_I(module, message, arg_cnt, ...) \
do { \
    ATTR_LOG_STRING msg_id_string[] = "[M:" #module " C:info ]: " message; \
    print_module_msgid_log_info((arg_cnt), (msg_id_string), ##__VA_ARGS__); \
} while (0)
```

在编译后，我们使用 **objdump** 工具，将 **.log_string** 段的 **格式字符串** 保存到单独的字符串文件 **log.str** 中。

```
@objcopy --only-section=.log_string -O binary example log.str
```

最终，生成的字符串文件 **log.str** 如下图：

 ![log.str File][2]

### 编码

宏 **LOG_MSGID_I** 负责将日志信息按照约定的输出格式进行编码。编码的格式可以自由定制，例子中的格式如下:

 ![Log Encoder][3]

编码的具体实现可以参见例子中的文件 **encoder.c**，编码后的输出如下：

 ![Encoded stream][4]

### 解码

上位机接收到编码过的日志信息后，根据字符串文件 **log.str** 中的 **格式字符串** 的描述进行解码，即可得到原始的日志信息。

完整的例子可以参见 [Log Compress][5]

 [1]: ./images/introduction.jpg
 [2]: ./images/log.str.jpg
 [3]: ./images/log_encoder.jpg
 [4]: ./images/encoded_stream.jpg
 [5]: ./demos
