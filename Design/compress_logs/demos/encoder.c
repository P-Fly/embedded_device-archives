#include "syslog.h"
#include "base.h"

extern const char __log_string_start[];

static uint8_t trace_buff[512];

void print_module_msgid_log_info(uint32_t arg_cnt, const char *format, ...)
{
    va_list args;
    uint32_t i;
    uint8_t label[2] = {0, 0};
    const char* p = format;
    packet_header_t* header = (packet_header_t*)trace_buff;
    uint32_t type;
    uint32_t offset = sizeof(packet_header_t);

    memset(trace_buff, 0, sizeof(trace_buff));

    va_start(args, format);
    for (i = 0; i < arg_cnt; i++)
    {
        while(1)
        {
            if(*p == '\0')
            {
                assert(0);
            }

            label[0] = label[1];
            label[1] = *p++;

            if (label[0] == '%')
            {
                switch(label[1])
                {
                case 'c':
                case 'd':
                case 'x':
                case 'X':
                {
                    uint32_t val = va_arg(args, uint32_t);
                    data_item_t* item = (data_item_t*)(trace_buff + offset);
                    item->id = DATA_TYPE_INT;
                    item->length = sizeof(val);
                    memcpy(item->payload, &val, item->length);
                    offset += item->length + sizeof(data_item_t);
                }
                break;

                case 's':
                {
                    char* str = va_arg(args, char*);
                    data_item_t* item = (data_item_t*)(trace_buff + offset);
                    item->id = DATA_TYPE_STRING;
                    item->length = strlen(str) + 1;
                    strncpy(item->payload, str, item->length);
                    offset += item->length + sizeof(data_item_t);
                }
                break;

                default:
                    assert(0);
                }

                break;
            }
        }
    }

    va_end(args);

    header->identifier = DEF_IDENTIFIER;
    header->length = offset - offsetof(packet_header_t, length) - sizeof(header->length);
    header->offset = format - __log_string_start;
    header->arg_cnt = arg_cnt;

#if 0
    for (i = 0; i < offset; i++)
    {
        printf("0x%x ", trace_buff[i]);
    }
    printf("\n");
#else
    uint32_t size = fwrite(trace_buff, offset, 1, stdout);
    assert(size != offset);
#endif
}
