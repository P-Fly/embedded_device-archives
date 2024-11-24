#ifndef __BASE_H__
#define __BASE_H__

#define DEF_IDENTIFIER 0xAA55

typedef enum
{
	DATA_TYPE_INT = 1,
	DATA_TYPE_STRING = 2,
} data_type;

typedef struct
{
    uint16_t identifier;
    uint16_t length;
    uint32_t offset;
    uint8_t arg_cnt;
	uint8_t payload[0];
} PACKED packet_header_t;

typedef struct
{
    uint8_t id;
    uint8_t length;
    uint8_t payload[0];
} PACKED data_item_t;

#define offsetof(struct_type, member_name) ((size_t) & ((struct_type *)0)->member_name )

#endif /* __BASE_H__ */
