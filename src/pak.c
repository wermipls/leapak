#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include "stream.h"
#include "block.h"

static inline size_t smin(size_t a, size_t b)
{
    return a > b ? b : a;
}

static inline size_t smax(size_t a, size_t b)
{
    return a < b ? b : a;
}

typedef struct match
{
    size_t pos;
    size_t len;
} match_t;

static inline size_t compare(uint8_t *a, uint8_t *b, size_t bytes)
{
    size_t i;

    for (i = 0; i < bytes; i++) {
        if (*a != *b) {
            break;
        }
        a++;
        b++;
    }

    return i;
}

static inline int is_offset_valid(size_t offset, size_t length)
{
    size_t max_offset = OFFSET_MAX + length;
    return offset <= max_offset;
}

match_t find_best_match(stream_t *s)
{
    size_t maxlen = smin(LENGTH_MAX, s->pos);
    match_t best = {0};

    size_t start = s->pos - smin(OFFSET_MAX + LENGTH_MAX, s->pos);
    size_t end = s->pos;
    size_t remaining = s->len - s->pos;

    for (size_t i = start; i < end; i++) {
        size_t max_match = smin(remaining, end - i);
        max_match = smin(max_match, maxlen);

        size_t match = compare(
            &s->data[i],
            &s->data[s->pos],
            max_match
        );

        if (match == 0) {
            continue;
        }

        if (!is_offset_valid(end - i, match)) {
            continue;
        }

        if (match > best.len) {
            //printf("found match: pos %8d, i %8d, len %4d\n", s->pos, i, match);
            best.len = match;
            best.pos = i;
        }
    }

    return best;
}

int write_bits(FILE *f, uint8_t data, int bits)
{
    static uint16_t remainder = 0;
    static int bitpos = 0;

    remainder |= ((data & ((1 << bits) - 1)) << bitpos);
    bitpos += bits;
    if (bitpos >= 8) {
        uint8_t byte = remainder;
        fwrite(&byte, 1, 1, f);
        remainder >>= 8;
        bitpos -= 8;
        return 8;
    }

    return 0;
}

int write_block(FILE *f, block_t b)
{
    int t = 0;
    t += write_bits(f, b.type, TYPE_BITS);

    switch (b.type)
    {
    case BTYPE_PASS4B:
        t += write_bits(f, b.data[0], 8);
        t += write_bits(f, b.data[1], 8);
        t += write_bits(f, b.data[2], 8);
        t += write_bits(f, b.data[3], 8);
        break;
    case BTYPE_PASS2B:
        t += write_bits(f, b.data[0], 8);
        t += write_bits(f, b.data[1], 8);
        break;
    case BTYPE_PASS1B:
        t += write_bits(f, b.data[0], 8);
        break;
    case BTYPE_MATCH:
        t += write_bits(f, b.offset >> 8, (OFFSET_BITS - 8));
        t += write_bits(f, b.offset, 8);
        t += write_bits(f, b.len, LENGTH_BITS);
        break;
    }

    return t;
}

int main(int argc, char *argv[])
{
    printf("Hi Lea\n");

    if (argc == 0) {
        printf("Lol\n");
        return -1;
    }

    if (argc != 3) {
        printf("usage: %s <input> <output>\n", argv[0]);
        return -2;
    }

    char *input  = argv[1];
    char *output = argv[2];

    stream_t *s = stream_read(input);
    if (!s) {
        printf("failed to read file '%s'\n", input);
        return -3;
    }

    if (s->len <= 16) {
        printf("file smaller or equals 16b, aborting...\n");
        return -4;
    }

    block_t *blk = malloc(s->len * sizeof(block_t)); // lazy...
    if (!blk) {
        printf("failed to malloc block data...\n");
        return -5;
    }
    size_t curblk = 0;

    blk[curblk].type = BTYPE_PASS4B;
    blk[curblk].data[0] = s->data[0];
    blk[curblk].data[1] = s->data[1];
    blk[curblk].data[2] = s->data[2];
    blk[curblk].data[3] = s->data[3];

    curblk += 1;
    s->pos += 4;

    while (s->pos < s->len) {
        match_t best = find_best_match(s);
        if (best.len < 3) {
            blk[curblk].type = BTYPE_PASS1B;
            blk[curblk].data[0] = s->data[s->pos];
            s->pos++;
            curblk++;
            continue;
        }

        blk[curblk].type = BTYPE_MATCH;
        blk[curblk].offset = s->pos - (best.pos + best.len);
        blk[curblk].len = best.len - 1;
        s->pos += best.len;
        curblk++;
    }

    size_t final_bitcount = 0;

    FILE *f = fopen(output, "wb");

    uint32_t length = s->len;
    fwrite(&length, 1, 4, f);

    for (size_t i = 0; i <= curblk; i++) {
        final_bitcount += write_block(f, blk[i]);
    }

    final_bitcount += write_bits(f, 0, 8); // flush remaining bits

    fclose(f);

    printf("encoded size: %d bytes\n", final_bitcount / 8);
    return 0;
}
