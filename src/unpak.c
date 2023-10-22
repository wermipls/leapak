#include "stream.h"
#include "block.h"

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
    }

    uint32_t length = *(uint32_t *)s->data; // hack
    printf("original length: %d bytes\n", length);
    s->pos += 4;

    FILE *f = fopen(output, "wb+");

    block_t b = {0};

    while (length) {
        b.type = stream_read_bits(s, TYPE_BITS);
        switch (b.type)
        {
        case BTYPE_MATCH:
            b.offset = stream_read_bits(s, OFFSET_BITS-8) << 8;
            b.offset |= stream_read_bits(s, 8);
            b.len = stream_read_bits(s, LENGTH_BITS);

            size_t pos = ftell(f);
            int offset = b.offset + b.len + 1;
            fseek(f, -offset, SEEK_CUR);

            uint8_t data[LENGTH_MAX]; // lazy hack
            fread(data, 1, b.len+1, f);

            fseek(f, pos, SEEK_SET);
            fwrite(data, 1, b.len+1, f);

            length -= b.len+1;
            break;
        case BTYPE_PASS1B:
            b.data[0] = stream_read_bits(s, 8);
            fwrite(b.data, 1, 1, f);
            length -= 1;
            break;
        case BTYPE_PASS2B:
            b.data[0] = stream_read_bits(s, 8);
            b.data[1] = stream_read_bits(s, 8);
            fwrite(b.data, 1, 2, f);
            length -= 2;
            break;
        case BTYPE_PASS4B:
            b.data[0] = stream_read_bits(s, 8);
            b.data[1] = stream_read_bits(s, 8);
            b.data[2] = stream_read_bits(s, 8);
            b.data[3] = stream_read_bits(s, 8);
            fwrite(b.data, 1, 4, f);
            length -= 4;
            break;
        }
    }

    fclose(f);
}
