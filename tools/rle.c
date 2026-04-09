/*
 * rle — PackBits-style RLE compressor for binary files
 *
 * Compress:    rle <input.bin> <output.rle>
 * Decompress:  rle -d <input.rle> <output.bin>
 *
 * Format:
 *   [4 bytes] magic "RLE0"
 *   [4 bytes] original size (uint32_t LE)
 *   [stream]  PackBits-style control bytes:
 *     n  0..127  → copy next (n+1) bytes verbatim
 *     n  129..255 → repeat next byte (257-n) times  [2..128 repetitions]
 *     n  128     → unused
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static const char MAGIC[4] = {'R','L','E','0'};

static uint8_t *read_file(const char *path, uint32_t *size_out) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open: %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); *size_out = 0; return calloc(1, 1); }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); free(buf); return NULL;
    }
    fclose(f);
    *size_out = (uint32_t)sz;
    return buf;
}

static void emit_literals(FILE *f, const uint8_t *buf, int n) {
    while (n > 0) {
        int chunk = n > 128 ? 128 : n;
        fputc(chunk - 1, f);          /* 0..127 */
        fwrite(buf, 1, (size_t)chunk, f);
        buf += chunk;
        n   -= chunk;
    }
}

static void emit_run(FILE *f, uint8_t byte, int n) {
    while (n > 0) {
        int chunk = n > 128 ? 128 : n;
        fputc(257 - chunk, f);        /* 129..255 */
        fputc(byte, f);
        n -= chunk;
    }
}

static int do_compress(const char *in_path, const char *out_path) {
    uint32_t in_size;
    uint8_t *data = read_file(in_path, &in_size);
    if (!data) return 1;

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "cannot open output: %s\n", out_path);
        free(data);
        return 1;
    }

    fwrite(MAGIC, 1, 4, out);
    fwrite(&in_size, 4, 1, out);

    uint8_t lit_buf[128];
    int     lit_n = 0;
    uint32_t i    = 0;

    while (i < in_size) {
        /* Count run length at position i */
        uint32_t run = 1;
        while (i + run < in_size && data[i + run] == data[i] && run < 128)
            run++;

        if (run >= 2) {
            /* Flush pending literals, then emit run */
            emit_literals(out, lit_buf, lit_n);
            lit_n = 0;
            emit_run(out, data[i], (int)run);
            i += run;
        } else {
            /* Queue one literal */
            if (lit_n == 128) {
                emit_literals(out, lit_buf, lit_n);
                lit_n = 0;
            }
            lit_buf[lit_n++] = data[i++];
        }
    }
    emit_literals(out, lit_buf, lit_n);

    fclose(out);

    long out_size = 0;
    { FILE *t = fopen(out_path, "rb"); if (t) { fseek(t,0,SEEK_END); out_size=ftell(t); fclose(t); } }
    printf("compressed %s -> %s  [%u -> %ld bytes, %.1f%%]\n",
           in_path, out_path, in_size, out_size,
           in_size ? 100.0 * out_size / in_size : 0.0);

    free(data);
    return 0;
}

static int do_decompress(const char *in_path, const char *out_path) {
    uint32_t in_size;
    uint8_t *data = read_file(in_path, &in_size);
    if (!data) return 1;

    if (in_size < 8 || memcmp(data, MAGIC, 4) != 0) {
        fprintf(stderr, "not an RLE0 file: %s\n", in_path);
        free(data);
        return 1;
    }

    uint32_t orig_size;
    memcpy(&orig_size, data + 4, 4);

    uint8_t *out_buf = malloc(orig_size ? orig_size : 1);
    if (!out_buf) { free(data); return 1; }

    uint32_t rpos = 8;     /* read position in compressed stream */
    uint32_t wpos = 0;     /* write position in output */

    while (rpos < in_size && wpos < orig_size) {
        uint8_t ctrl = data[rpos++];
        if (ctrl <= 127) {
            /* Literal run: copy next (ctrl+1) bytes */
            uint32_t n = (uint32_t)ctrl + 1;
            if (rpos + n > in_size) n = in_size - rpos;
            if (wpos + n > orig_size) n = orig_size - wpos;
            memcpy(out_buf + wpos, data + rpos, n);
            wpos += n;
            rpos += n;
        } else if (ctrl >= 129) {
            /* Repeat run: repeat next byte (257-ctrl) times */
            uint32_t n = 257 - ctrl;
            if (rpos >= in_size) break;
            uint8_t byte = data[rpos++];
            if (wpos + n > orig_size) n = orig_size - wpos;
            memset(out_buf + wpos, byte, n);
            wpos += n;
        }
        /* ctrl == 128: no-op */
    }

    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "cannot open output: %s\n", out_path);
        free(out_buf); free(data);
        return 1;
    }
    fwrite(out_buf, 1, wpos, f);
    fclose(f);

    printf("decompressed %s -> %s  [%ld -> %u bytes]\n",
           in_path, out_path, (long)in_size, wpos);

    free(out_buf);
    free(data);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "-d") == 0)
        return do_decompress(argv[2], argv[3]);
    if (argc == 3 && argv[1][0] != '-')
        return do_compress(argv[1], argv[2]);

    fprintf(stderr,
        "usage:\n"
        "  compress:   rle <input.bin> <output.rle>\n"
        "  decompress: rle -d <input.rle> <output.bin>\n");
    return 1;
}
