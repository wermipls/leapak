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

match_t prev_match[PREV_MATCH_MAX];
size_t prev_match_pos = 0;
size_t prev_match_count = 0;

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

static inline int is_offset_valid(size_t offset, size_t length, int max_offset)
{
    size_t max = max_offset + length;
    return offset < max;
}

match_t find_best_match(stream_t *s, int max_len, int max_offset)
{
    size_t maxlen = smin(max_len, s->pos);
    match_t best = {0};

    size_t start = s->pos - smin(max_offset + max_len, s->pos);
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

        if (!is_offset_valid(end - i, match, max_offset)) {
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

int try_previous_matches(stream_t *s)
{
    int best_valid = -1;
    
    for (size_t i = 0; i < smin(PREV_MATCH_MAX, prev_match_count); i++) {
        match_t cur = prev_match[i];

        if (cur.len > s->len - s->pos) {
            continue;
        }

        size_t count = compare(
            &s->data[cur.pos],
            &s->data[s->pos],
            cur.len
        );

        if (count != cur.len) {
            continue;
        }

        if (best_valid >= 0) {
            match_t best = prev_match[best_valid];

            if (best.len < cur.len) {
                best_valid = i;
            }
        } else {
            best_valid = i;
        }
    }

    return best_valid;
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

size_t command_counts[5] = {0,0,0,0,0};

int write_block(FILE *f, block_t b)
{
    int t = 0;
    if (b.type >= 0b110) {
        t += write_bits(f, b.type>>1, 2);
        t += write_bits(f, b.type, 1);
    } else {
        t += write_bits(f, b.type, 2);
    }
    command_counts[b.type]++;

    switch (b.type)
    {
    case BTYPE_PASSMANY:
        t += write_bits(f, b.len, PASS_BITS);
        for (int i = 0; i <= (b.len+1); i++) {
            t += write_bits(f, b.data[i], 8);
        }
        break;
    case BTYPE_PASS1B:
        t += write_bits(f, b.data[0], 8);
        break;
    case BTYPE_MATCH:
        t += write_bits(f, b.offset >> 8, (LONG_OFFSET_BITS - 8));
        t += write_bits(f, b.offset, 8);
        t += write_bits(f, b.len, LONG_LENGTH_BITS);
        break;
    case BTYPE_MATCH_SHORT:
        t += write_bits(f, b.offset, SHORT_OFFSET_BITS);
        t += write_bits(f, b.len, SHORT_LENGTH_BITS);
        break;
    case BTYPE_PREV_MATCH:
        t += write_bits(f, b.offset, PREV_MATCH_BITS);
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

    blk[curblk].type = BTYPE_PASSMANY;
    blk[curblk].len = 4-2;
    blk[curblk].data[0] = s->data[0];
    blk[curblk].data[1] = s->data[1];
    blk[curblk].data[2] = s->data[2];
    blk[curblk].data[3] = s->data[3];

    curblk += 1;
    s->pos += 4;

    while (s->pos < s->len) {
        if ((s->pos & ((1<<20)-1)) == 0) {
            printf("%d bytes...\n", s->pos);
        }

        match_t best_long = find_best_match(s, LONG_LENGTH_MAX, LONG_OFFSET_MAX);
        match_t best_short = find_best_match(s, SHORT_LENGTH_MAX, SHORT_OFFSET_MAX);
        match_t best;
        int is_long;

        if (best_short.len+1 >= best_long.len) {
            best = best_short;
            is_long = 0;
        } else {
            best = best_long;
            is_long = 1;
        }

        int best_prev_idx = try_previous_matches(s);
        if (best_prev_idx >= 0) {
            match_t best_prev = prev_match[best_prev_idx];

            if (best_prev.len+1 >= best.len) {
                blk[curblk].type = BTYPE_PREV_MATCH;
                blk[curblk].offset = best_prev_idx;
                s->pos += best_prev.len;
                curblk++;
                continue;
            }
        }

        if (best.len < 3) {
            blk[curblk].type = BTYPE_PASS1B;
            blk[curblk].data[0] = s->data[s->pos];
            s->pos++;
            curblk++;
            continue;
        }

        prev_match[prev_match_pos] = best;
        prev_match_pos = (prev_match_pos + 1) & (PREV_MATCH_MAX - 1);
        prev_match_count++;

        blk[curblk].type = is_long ? BTYPE_MATCH : BTYPE_MATCH_SHORT;
        blk[curblk].offset = s->pos - (best.pos + best.len);
        blk[curblk].len = best.len - 1;
        s->pos += best.len;
        curblk++;
    }

    size_t final_bitcount = 0;

    FILE *f = fopen(output, "wb");

    uint32_t length = s->len;
    fwrite(&length, 1, 4, f);

    int last_pass = -1; 

    for (size_t i = 0; i < curblk; i++) {
        if (blk[i].type == BTYPE_PASS1B) {
            if (last_pass < 0) {
                last_pass = i;
                continue;
            }

            if ((i - last_pass) < PASS_MAX) {
                blk[last_pass].type = BTYPE_PASSMANY;
                blk[last_pass].data[i-last_pass] = blk[i].data[0];
                blk[last_pass].len = i-last_pass-1;

                continue;
            }
        } 
        
        if (last_pass >= 0) {
            final_bitcount += write_block(f, blk[last_pass]);
            last_pass = -1;
        }

        final_bitcount += write_block(f, blk[i]);
    }

    if (last_pass >= 0) {
        final_bitcount += write_block(f, blk[last_pass]);
    }

    final_bitcount += write_bits(f, 0, 8); // flush remaining bits

    fclose(f);

    printf("encoded size: %d bytes\n", final_bitcount / 8);
    return 0;
}
