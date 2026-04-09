/*
 * img_conv - convert PNG to packed palette-indexed binary, and back
 *
 * Encode:
 *   img_conv <input.png> <output.bin> <transparent_hex> [index:hex ...]
 *
 * Decode:
 *   img_conv -d <width> <height> <input.bin> <output.png> <transparent_hex> [index:hex ...]
 *
 * Examples:
 *   img_conv hero.png hero.bin 00ff00 1:ff0000 2:ffffff 3:0000ff
 *   img_conv -d 8 8 hero.bin hero.png 00ff00 1:ff0000 2:ffffff 3:0000ff
 *
 * Binary format:
 *   Raw packed bits, MSB-first within each byte, row-major order.
 *   Bits per pixel = ceil(log2(n_colors)), where n_colors = max_index + 1.
 *   Index 0 is reserved for transparent (exact match to transparent_hex).
 *   Non-transparent pixels are mapped to the nearest palette entry by
 *   perceptual (redmean) RGB distance.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define MAX_COLORS 64

static int bits_needed(int n) {
    int b = 1;
    while ((1 << b) < n) b++;
    return b;
}

/* Redmean perceptual distance — weights shift with how red the midpoint is.
 * Scaled by 256 to stay in integer arithmetic. */
static int rgb_dist2(int r1, int g1, int b1, int r2, int g2, int b2) {
    int dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
    int rm = (r1 + r2);          /* 2 * mean red, range 0..510 */
    return (512 + rm) * dr*dr + 1024 * dg*dg + (768 - rm) * db*db;
}

static int parse_palette(int argc, char **argv, int start,
                         uint8_t *pal_r, uint8_t *pal_g, uint8_t *pal_b,
                         int *pal_valid, int *n_colors) {
    for (int i = start; i < argc; i++) {
        int idx;
        unsigned int cr, cg, cb;
        if (sscanf(argv[i], "%d:%02x%02x%02x", &idx, &cr, &cg, &cb) != 4) {
            fprintf(stderr, "bad palette entry: %s  (expected index:rrggbb)\n", argv[i]);
            return 0;
        }
        if (idx <= 0 || idx >= MAX_COLORS) {
            fprintf(stderr, "palette index out of range: %d\n", idx);
            return 0;
        }
        pal_r[idx] = (uint8_t)cr;
        pal_g[idx] = (uint8_t)cg;
        pal_b[idx] = (uint8_t)cb;
        pal_valid[idx] = 1;
        if (idx + 1 > *n_colors) *n_colors = idx + 1;
    }
    return 1;
}

static int do_encode(int argc, char **argv) {
    /* argv: in.png out.bin transparent_hex [index:hex ...] */
    if (argc < 3) {
        fprintf(stderr, "encode usage: img_conv <in.png> <out.bin> <transparent_hex> [index:hex ...]\n");
        return 1;
    }

    const char *in_path   = argv[0];
    const char *out_path  = argv[1];
    const char *trans_str = argv[2];

    unsigned int tr, tg, tb;
    if (sscanf(trans_str, "%02x%02x%02x", &tr, &tg, &tb) != 3) {
        fprintf(stderr, "bad transparent color: %s\n", trans_str);
        return 1;
    }

    uint8_t pal_r[MAX_COLORS] = {0};
    uint8_t pal_g[MAX_COLORS] = {0};
    uint8_t pal_b[MAX_COLORS] = {0};
    int     pal_valid[MAX_COLORS] = {0};
    int     n_colors = 1;

    if (!parse_palette(argc, argv, 3, pal_r, pal_g, pal_b, pal_valid, &n_colors))
        return 1;

    int bpp  = bits_needed(n_colors);
    int mask = (1 << bpp) - 1;

    int w, h, channels;
    unsigned char *img = stbi_load(in_path, &w, &h, &channels, 4);
    if (!img) {
        fprintf(stderr, "failed to load image: %s\n", in_path);
        return 1;
    }

    uint8_t *indices = malloc((size_t)w * h);
    if (!indices) { stbi_image_free(img); return 1; }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned char *px = img + (y * w + x) * 4;
            int r = px[0], g = px[1], b = px[2], a = px[3];

            if (a == 0 || (r == (int)tr && g == (int)tg && b == (int)tb)) {
                indices[y * w + x] = 0;
                continue;
            }

            int best_idx  = 1;
            int best_dist = INT_MAX;
            for (int ci = 1; ci < n_colors; ci++) {
                if (!pal_valid[ci]) continue;
                int d = rgb_dist2(r, g, b, pal_r[ci], pal_g[ci], pal_b[ci]);
                if (d < best_dist) { best_dist = d; best_idx = ci; }
            }
            indices[y * w + x] = (uint8_t)best_idx;
        }
    }
    stbi_image_free(img);

    int total_pixels = w * h;
    int total_bytes  = (total_pixels * bpp + 7) / 8;

    uint8_t *out = calloc(1, (size_t)total_bytes);
    if (!out) { free(indices); return 1; }

    int bit_pos = 0;
    for (int i = 0; i < total_pixels; i++) {
        int val = indices[i] & mask;
        for (int b = bpp - 1; b >= 0; b--) {
            if ((val >> b) & 1)
                out[bit_pos / 8] |= (uint8_t)(1 << (7 - (bit_pos % 8)));
            bit_pos++;
        }
    }
    free(indices);

    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "failed to open output: %s\n", out_path);
        free(out);
        return 1;
    }
    fwrite(out, 1, (size_t)total_bytes, f);
    fclose(f);
    free(out);

    printf("encoded %s -> %s  [%dx%d, %d colors, %d bpp, %d bytes]\n",
           in_path, out_path, w, h, n_colors, bpp, total_bytes);
    return 0;
}

static int do_decode(int argc, char **argv) {
    /* argv: width height in.bin out.png transparent_hex [index:hex ...] */
    if (argc < 5) {
        fprintf(stderr, "decode usage: img_conv -d <width> <height> <in.bin> <out.png> <transparent_hex> [index:hex ...]\n");
        return 1;
    }

    int w = atoi(argv[0]);
    int h = atoi(argv[1]);
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "invalid dimensions: %s x %s\n", argv[0], argv[1]);
        return 1;
    }

    const char *in_path   = argv[2];
    const char *out_path  = argv[3];
    const char *trans_str = argv[4];

    unsigned int tr, tg, tb;
    if (sscanf(trans_str, "%02x%02x%02x", &tr, &tg, &tb) != 3) {
        fprintf(stderr, "bad transparent color: %s\n", trans_str);
        return 1;
    }

    uint8_t pal_r[MAX_COLORS] = {0};
    uint8_t pal_g[MAX_COLORS] = {0};
    uint8_t pal_b[MAX_COLORS] = {0};
    int     pal_valid[MAX_COLORS] = {0};
    int     n_colors = 1;

    if (!parse_palette(argc, argv, 5, pal_r, pal_g, pal_b, pal_valid, &n_colors))
        return 1;

    int bpp  = bits_needed(n_colors);
    int mask = (1 << bpp) - 1;

    /* Read binary file */
    FILE *f = fopen(in_path, "rb");
    if (!f) {
        fprintf(stderr, "failed to open input: %s\n", in_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int expected_bytes = (w * h * bpp + 7) / 8;
    if (file_size < expected_bytes) {
        fprintf(stderr, "file too small: expected %d bytes for %dx%d at %d bpp, got %ld\n",
                expected_bytes, w, h, bpp, file_size);
        fclose(f);
        return 1;
    }

    uint8_t *packed = malloc((size_t)file_size);
    if (!packed) { fclose(f); return 1; }
    if (fread(packed, 1, (size_t)file_size, f) != (size_t)file_size) {
        fprintf(stderr, "failed to read input: %s\n", in_path);
        free(packed); fclose(f);
        return 1;
    }
    fclose(f);

    /* Unpack bits into RGBA pixels */
    int total_pixels = w * h;
    uint8_t *rgba = malloc((size_t)total_pixels * 4);
    if (!rgba) { free(packed); return 1; }

    int bit_pos = 0;
    for (int i = 0; i < total_pixels; i++) {
        int val = 0;
        for (int b = bpp - 1; b >= 0; b--) {
            if ((packed[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1)
                val |= (1 << b);
            bit_pos++;
        }
        val &= mask;

        uint8_t *px = rgba + i * 4;
        if (val == 0) {
            /* Transparent — write the key color at full alpha so the result
             * is viewable, but mark alpha=0 for proper transparency. */
            px[0] = (uint8_t)tr;
            px[1] = (uint8_t)tg;
            px[2] = (uint8_t)tb;
            px[3] = 0;
        } else if (pal_valid[val]) {
            px[0] = pal_r[val];
            px[1] = pal_g[val];
            px[2] = pal_b[val];
            px[3] = 255;
        } else {
            /* Unknown index — magenta so it's obvious */
            px[0] = 255; px[1] = 0; px[2] = 255; px[3] = 255;
        }
    }
    free(packed);

    if (!stbi_write_png(out_path, w, h, 4, rgba, w * 4)) {
        fprintf(stderr, "failed to write PNG: %s\n", out_path);
        free(rgba);
        return 1;
    }
    free(rgba);

    printf("decoded %s -> %s  [%dx%d, %d colors, %d bpp]\n",
           in_path, out_path, w, h, n_colors, bpp);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "-d") == 0)
        return do_decode(argc - 2, argv + 2);
    if (argc >= 2 && argv[1][0] != '-')
        return do_encode(argc - 1, argv + 1);

    fprintf(stderr,
        "usage:\n"
        "  encode: img_conv <in.png> <out.bin> <transparent_hex> [index:hex ...]\n"
        "  decode: img_conv -d <width> <height> <in.bin> <out.png> <transparent_hex> [index:hex ...]\n");
    return 1;
}
