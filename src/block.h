#pragma once

#include <stdint.h>
#include <stddef.h>

#define OFFSET_BITS 13
#define LENGTH_BITS 5
#define OFFSET_MAX (1 << OFFSET_BITS)
#define LENGTH_MAX (1 << LENGTH_BITS)

enum btype
{
    BTYPE_PASS1B,
    BTYPE_PASS2B,
    BTYPE_PASS4B,
    BTYPE_MATCH,
};

#define TYPE_BITS 2

typedef struct block
{
    enum btype type : TYPE_BITS;
    union {
        struct {
            uint16_t offset : OFFSET_BITS;
            uint8_t len     : LENGTH_BITS;
        };
        struct {
            uint8_t data[4];
        };
    };
} block_t;
