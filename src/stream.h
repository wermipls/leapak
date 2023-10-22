#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

typedef struct stream
{
    uint8_t *data;
    size_t len;
    size_t pos;
    size_t bitpos;
} stream_t;

stream_t *stream_read(const char *fname)
{
    FILE *f = fopen(fname, "rb");
    if (!f) {
        return NULL;
    }

    stream_t *s = malloc(sizeof(stream_t));
    if (!s) {
        fclose(f);
        return NULL;
    }

    s->pos = 0;

    fseek(f, 0, SEEK_END);
    s->len = ftell(f);
    fseek(f, 0, SEEK_SET);

    s->data = malloc(s->len);
    if (!s->data) {
        fclose(f);
        free(s);
        return NULL;
    }

    size_t size = fread(s->data, 1, s->len, f);
    if (size != s->len) {
        fclose(f);
        free(s->data);
        free(s);
        return NULL;
    }

    fclose(f);
    return s;
}

void stream_free(stream_t *s)
{
    if (!s) {
        return;
    }

    if (s->data) {
        free(s->data);
    }
    free(s);
}

uint8_t stream_read_bits(stream_t *s, int bits)
{
    uint16_t data = (s->data[s->pos+1] << 8) | s->data[s->pos];
    data >>= s->bitpos;
    data &= (1 << bits) - 1;
    s->bitpos += bits;
    if (s->bitpos >= 8) {
        s->pos++;
        s->bitpos -= 8;
    }

    return data;
}
