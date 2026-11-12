#include "syslog.h"
#include "base.h"

static uint8_t trace_buff[512];
static uint8_t raw_buff[512];
static uint8_t str_buff[512];

void main(void)
{
    uint32_t i = 0;
    uint32_t j = 0;
    uint8_t* trace_ptr;
    uint32_t trace_offset = 0;
    uint8_t* raw_ptr = 0;
    uint32_t raw_size = 0;
    uint32_t raw_offset = 0;
    uint8_t* str_ptr;
    uint32_t str_size = 0;
    uint32_t str_offset = 0;
    uint8_t label[2] = {0, 0};
    uint8_t arg_cnt;
    uint8_t arg_index;
    data_item_t* item;

    raw_size = fread(raw_buff, 1, sizeof(raw_buff), stdin);
#if 0
    printf("raw_size = %d\n", raw_size);
    for (i = 0; i < raw_size; i++)
    {
        printf("0x%x ", raw_buff[i]);
    }
    printf("\n");
#endif
    FILE *file = fopen("log.str", "rb");
    assert(file);

    str_size = fread(str_buff, 1, sizeof(str_buff), file);
#if 0
    printf("str_size = %d\n", str_size);
    for (i = 0; i < str_size; i++)
    {
        printf("0x%x ", str_buff[i]);
    }
    printf("\n");
#endif
    fclose(file);

    while(raw_offset < raw_size)
    {
        packet_header_t* header = (packet_header_t*)(raw_buff + raw_offset);

        if (header->identifier != DEF_IDENTIFIER)
        {
            printf("Identifier invalid\n");
            return;
        }

        str_offset = header->offset;
        arg_cnt = header->arg_cnt;
        arg_index = 0;

        str_ptr = str_buff + str_offset;
        trace_ptr = trace_buff;

        while(*str_ptr != '\0')
        {
            label[0] = label[1];
            label[1] = *str_ptr;

            *trace_ptr++ = *str_ptr++;

            if (label[0] == '%')
            {
                *(--trace_ptr) = 0;
                *(--trace_ptr) = 0;

                if(arg_cnt == arg_index)
                {
                    assert(0);
                }

                raw_ptr = header->payload;

                for(i = 0; i < arg_index; i++)
                {
                    item = (data_item_t*)raw_ptr;
                    raw_ptr += sizeof(data_item_t) + item->length;
                }

                arg_index++;

                switch(label[1])
                {
                case 'c':
                {
                    data_item_t* item = (data_item_t*)raw_ptr;
                    char value = *(item->payload);
                    int32_t len = sprintf(trace_ptr, "%c", value);
                    trace_ptr += len;
                }
                break;

                case 'd':
                {
                    data_item_t* item = (data_item_t*)raw_ptr;
                    uint32_t value = *(item->payload);
                    int32_t len = sprintf(trace_ptr, "%d", value);
                    trace_ptr += len;
                }
                break;

                case 'x':
                {
                    data_item_t* item = (data_item_t*)raw_ptr;
                    uint32_t value = *(item->payload);
                    int32_t len = sprintf(trace_ptr, "%x", value);
                    trace_ptr += len;
                }
                break;

                case 'X':
                {
                    data_item_t* item = (data_item_t*)raw_ptr;
                    uint32_t value = *(item->payload);
                    int32_t len = sprintf(trace_ptr, "%X", value);
                    trace_ptr += len;
                }
                break;

                case 's':
                {
                    data_item_t* item = (data_item_t*)raw_ptr;
                    strncpy(trace_ptr, item->payload, item->length - 1);
                    trace_ptr += item->length - 1;
                }
                break;

                default:
                    assert(0);
                }

                label[0] = 0;
                label[1] = 0;
            }
        }

        *trace_ptr++ = 0;

        printf("%s", trace_buff);

        raw_offset += header->length + offsetof(packet_header_t, length) + sizeof(header->length);
    }
}
