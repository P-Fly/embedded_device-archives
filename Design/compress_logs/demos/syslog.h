#ifndef __SYSLOG_H__
#define __SYSLOG_H__

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

extern void print_module_msgid_log_info(uint32_t arg_cnt, const char *format, ...);

#define PACKED __attribute__((packed))
#define ATTR_LOG_STRING __attribute__ ((__section__(".log_string"))) static const char

#define LOG_MSGID_I(module, message, arg_cnt, ...) \
do { \
    ATTR_LOG_STRING msg_id_string[] = "[M:" #module " C:info ]: " message; \
    print_module_msgid_log_info((arg_cnt), (msg_id_string), ##__VA_ARGS__); \
} while (0)

#endif /* __SYSLOG_H__ */
