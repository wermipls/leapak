#pragma once

#include <stdint.h>
#include <stddef.h>

#define LONG_OFFSET_BITS 16
#define LONG_LENGTH_BITS 4
#define LONG_OFFSET_MAX (1 << LONG_OFFSET_BITS)
#define LONG_LENGTH_MAX (1 << LONG_LENGTH_BITS)

#define SHORT_OFFSET_BITS 8
#define SHORT_LENGTH_BITS 4
#define SHORT_OFFSET_MAX (1 << SHORT_OFFSET_BITS)
#define SHORT_LENGTH_MAX (1 << SHORT_LENGTH_BITS)

#define PASS_BITS 2
#define PASS_MAX ((1 << PASS_BITS) + 1)

#define PREV_MATCH_BITS 3
#define PREV_MATCH_MAX (1 << PREV_MATCH_BITS)

enum btype
{
    BTYPE_PASS1B        = 0b00,
    BTYPE_MATCH_SHORT   = 0b01,
    BTYPE_MATCH         = 0b10,
    BTYPE_PASSMANY      = 0b110,
    BTYPE_PREV_MATCH    = 0b111,
};

#define TYPE_BITS 3

typedef struct block
{
    enum btype type : TYPE_BITS;
    uint8_t len;

    union {
        struct {
            uint16_t offset;
        };
        struct {
            uint8_t data[PASS_MAX];
        };
    };
} block_t;
