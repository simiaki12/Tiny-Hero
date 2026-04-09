/*
 * bw_conv — convert PNG to 1-bit RLE binary, and back
 *
 * Encode:  bw_conv <input.png> <output.bin>
 * Decode:  bw_conv -d <input.bin> <output.png>
 *
 * Binary format:
 *   [2 bytes] uint16_t width  (little-endian)
 *   [2 bytes] uint16_t height (little-endian)
 *   [N bytes] RLE stream: alternating run lengths (uint8_t) starting with
 *             white (0). 0 in stream = skip (0 pixels of this color, advance
 *             to other). Runs > 255 are split: 255, 0, remainder.
 *             Implicit end when width*height pixels are filled.
 *
 * Threshold: luminance = (r*299 + g*587 + b*114) / 1000
 *            >= 128 → white (0),  < 128 → black (1).  alpha = 0 → white.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint8_t to_bit(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (a == 0) return 0; /* transparent → white */
    int lum = ((int)r * 299 + (int)g * 587 + (int)b * 114) / 1000;
    return lum < 128 ? 1 : 0;
}

/* Write a run of `count` pixels of `bit` into the RLE stream.
   Handles runs > 255 by chaining with 0-skips. */
static void write_run(FILE *f, uint8_t cur_bit, uint8_t bit, int count) {
    /* If the bit we want to write is not the current color, emit a 0-skip first */
    if (bit != cur_bit) {
        fputc(0, f);
    }
    while (count > 0) {
        int chunk = count > 255 ? 255 : count;
        fputc(chunk, f);
        count -= chunk;
        if (count > 0) {
            fputc(0, f); /* 0-skip the other color */
        }
    }
}

static int do_encode(const char *in_path, const char *out_path) {
    int w, h, channels;
    unsigned char *img = stbi_load(in_path, &w, &h, &channels, 4);
    if (!img) {
        fprintf(stderr, "failed to load: %s\n", in_path);
        return 1;
    }

    int total = w * h;
    uint8_t *bits = malloc((size_t)total);
    if (!bits) { stbi_image_free(img); return 1; }

    for (int i = 0; i < total; i++) {
        unsigned char *px = img + i * 4;
        bits[i] = to_bit(px[0], px[1], px[2], px[3]);
    }
    stbi_image_free(img);

    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "failed to open output: %s\n", out_path);
        free(bits);
        return 1;
    }

    uint16_t uw = (uint16_t)w, uh = (uint16_t)h;
    fwrite(&uw, 2, 1, f);
    fwrite(&uh, 2, 1, f);

    /* RLE encode */
    uint8_t cur_bit = 0;  /* stream starts on white */
    int run = 0;
    uint8_t prev = bits[0];

    for (int i = 0; i < total; i++) {
        if (bits[i] == prev) {
            run++;
        } else {
            write_run(f, cur_bit, prev, run);
            cur_bit = prev ^ 1; /* stream always ends on the opposite color after a run */
            prev = bits[i];
            run = 1;
        }
    }
    /* flush last run */
    write_run(f, cur_bit, prev, run);

    fclose(f);
    free(bits);

    long out_size = 4; /* header */
    {
        FILE *tmp = fopen(out_path, "rb");
        if (tmp) { fseek(tmp, 0, SEEK_END); out_size = ftell(tmp); fclose(tmp); }
    }
    printf("encoded %s -> %s  [%dx%d, %ld bytes (uncompressed: %d)]\n",
           in_path, out_path, w, h, out_size, (w * h + 7) / 8);
    return 0;
}

static int do_decode(const char *in_path, const char *out_path) {
    FILE *f = fopen(in_path, "rb");
    if (!f) {
        fprintf(stderr, "failed to open: %s\n", in_path);
        return 1;
    }

    uint16_t w, h;
    if (fread(&w, 2, 1, f) != 1 || fread(&h, 2, 1, f) != 1) {
        fprintf(stderr, "truncated header\n");
        fclose(f);
        return 1;
    }

    int total = (int)w * h;
    uint8_t *bits = calloc(1, (size_t)total);
    if (!bits) { fclose(f); return 1; }

    int pos = 0;
    uint8_t cur = 0; /* current color: 0=white, 1=black */
    int byte;
    while (pos < total && (byte = fgetc(f)) != EOF) {
        int run = (uint8_t)byte;
        for (int i = 0; i < run && pos < total; i++)
            bits[pos++] = cur;
        cur ^= 1;
    }
    fclose(f);

    /* bits → RGBA */
    uint8_t *rgba = malloc((size_t)total * 4);
    if (!rgba) { free(bits); return 1; }
    for (int i = 0; i < total; i++) {
        uint8_t v = bits[i] ? 0 : 255;
        rgba[i*4+0] = v;
        rgba[i*4+1] = v;
        rgba[i*4+2] = v;
        rgba[i*4+3] = 255;
    }
    free(bits);

    if (!stbi_write_png(out_path, w, h, 4, rgba, w * 4)) {
        fprintf(stderr, "failed to write PNG: %s\n", out_path);
        free(rgba);
        return 1;
    }
    free(rgba);

    printf("decoded %s -> %s  [%dx%d]\n", in_path, out_path, w, h);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "-d") == 0)
        return do_decode(argv[2], argv[3]);
    if (argc == 3 && argv[1][0] != '-')
        return do_encode(argv[1], argv[2]);

    fprintf(stderr,
        "usage:\n"
        "  encode: bw_conv <input.png> <output.bin>\n"
        "  decode: bw_conv -d <input.bin> <output.png>\n");
    return 1;
}
