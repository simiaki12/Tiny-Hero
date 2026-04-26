/*
 * img_conv - PNG <-> self-contained palette-indexed binary
 *
 * Binary format:
 *   [u8 width] [u8 height] [s8 color_count]
 *   if color_count > 0: [color_count * 3 bytes: RGB palette]
 *   if color_count < 0: no palette bytes; built-in palette abs(color_count) is used
 *   [packed pixel indices, MSB-first within each byte]
 *
 *   bpp = ceil(log2(n_colors + 1))   where n_colors = palette size
 *   index n_colors = transparent pixel
 *
 * Built-in palettes (pass negative N to encoder):
 *   -1  Gameboy      4 colors
 *   -2  Grayscale    4 colors
 *   -3  CGA          16 colors
 *
 * Usage:
 *   img_conv <in.png> <out.bin> [N [preview.png]]
 *     N > 0: k-means quantize to N colors, store palette in file
 *     N < 0: snap to built-in palette |N|, no palette in file
 *   img_conv -d <in.bin> <out.png>
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

/* smallest b such that 2^b >= n */
static int bits_needed(int n) {
    int b = 1;
    while ((1 << b) < n) b++;
    return b;
}

/* Redmean perceptual distance squared — weights shift with how red the midpoint is. */
static long rgb_dist2(int r1, int g1, int b1, int r2, int g2, int b2) {
    int dr = r1-r2, dg = g1-g2, db = b1-b2;
    int rm = r1+r2; /* 2 * mean red, range 0..510 */
    return (long)(512+rm)*dr*dr + 1024L*dg*dg + (long)(768-rm)*db*db;
}

/* -------------------------------------------------------------------------
 * Built-in palettes
 * ------------------------------------------------------------------------- */

typedef struct { int n; uint8_t r[16], g[16], b[16]; } BuiltinPalette;

static const BuiltinPalette builtin_palettes[] = {
    /* 1 — Gameboy DMG */
    { 4,
      {15,  48, 139, 155},
      {56,  98, 172, 188},
      {15,  56, 123, 182} },
    /* 2 — 4-shade grayscale */
    { 4,
      {0,   85, 170, 255},
      {0,   85, 170, 255},
      {0,   85, 170, 255} },
    /* 3 — CGA palette 1 (high intensity) */
    { 16,
      {  0,  0,170,170,  0,  0,170,170, 85, 85,255,255, 85, 85,255,255},
      {  0,  0,  0,  0,170,170,170,170, 85, 85, 85, 85,255,255,255,255},
      {  0,170,  0,170,  0,170,  0,170,  0,170,  0,170,  0,170,  0,170} },
};
#define N_BUILTIN_PALETTES (int)(sizeof(builtin_palettes)/sizeof(builtin_palettes[0]))

/* Returns 1 if (x,y) is inside the tile diamond+walls for a tile of size w×h */
static int tile_in_bounds(int x, int y, int w, int h) {
    int dh = w/2, hdh = w/4, wh = h-dh, cx = w/2;
    int r = y+1;
    if (r >= 1 && r < dh) {
        int span = (r <= hdh ? r : dh-r) * (w/hdh);
        if (x >= cx-span/2 && x < cx+span/2) return 1;
    }
    if (wh > 0) {
        if (x < cx) { int ty = hdh+x/2; if (y >= ty && y < ty+wh) return 1; }
        else { int col=x-cx; int ty=dh-1-col/2; if (y >= ty && y < ty+wh) return 1; }
    }
    return 0;
}

/* Returns pointer to built-in palette for id 1..N_BUILTIN_PALETTES, NULL if unknown */
static const BuiltinPalette *get_builtin(int id) {
    if (id < 1 || id > N_BUILTIN_PALETTES) return NULL;
    return &builtin_palettes[id - 1];
}

/* Snap every opaque pixel in img to the nearest color in the given palette */
static void remap_to_palette(unsigned char *img, int total, const BuiltinPalette *pal) {
    for (int i = 0; i < total; i++) {
        if (img[i*4+3] < 25) continue;
        int r = img[i*4], g = img[i*4+1], b = img[i*4+2];
        long best_d = LONG_MAX; int best_c = 0;
        for (int c = 0; c < pal->n; c++) {
            long d = rgb_dist2(r, g, b, pal->r[c], pal->g[c], pal->b[c]);
            if (d < best_d) { best_d = d; best_c = c; }
        }
        img[i*4]   = pal->r[best_c];
        img[i*4+1] = pal->g[best_c];
        img[i*4+2] = pal->b[best_c];
    }
}

/*
 * K-means color quantization.
 * Modifies img (RGBA, total pixels) in-place: every opaque pixel's RGB is
 * replaced with its nearest cluster center.
 * Runs 16 restarts, each starting from a different pixel, keeps the one with
 * the lowest total distortion. Fixed (seeded) centers are never moved.
 */
#define KMEANS_RESTARTS 16

/* seed_r/g/b: arrays of length k; entry >= 0 pins that center, -1 = auto-init */
static void quantize(unsigned char *img, int total, int k,
                     const int *seed_r, const int *seed_g, const int *seed_b) {
    /* Collect opaque pixels */
    int n = 0;
    for (int i = 0; i < total; i++)
        if (img[i*4+3] >= 25) n++;
    if (n == 0) return;
    if (k > n) k = n;

    uint8_t *pr = malloc((size_t)n);
    uint8_t *pg = malloc((size_t)n);
    uint8_t *pb = malloc((size_t)n);
    int *src_idx = malloc((size_t)n * sizeof(int));
    int j = 0;
    for (int i = 0; i < total; i++) {
        if (img[i*4+3] >= 25) {
            pr[j] = img[i*4]; pg[j] = img[i*4+1]; pb[j] = img[i*4+2];
            src_idx[j] = i;
            j++;
        }
    }

    /* Precompute which centers are fixed (seeded) */
    int fixed[255] = {0};
    int n_fixed = 0;
    for (int ci = 0; ci < k; ci++) {
        if (seed_r[ci] >= 0) { fixed[ci] = 1; n_fixed++; }
    }
    /* If everything is seeded there's nothing to search over */
    int n_restarts = (n_fixed < k) ? KMEANS_RESTARTS : 1;

    uint8_t best_r[255], best_g[255], best_b[255];
    long    best_distortion = LONG_MAX;

    int *assign      = malloc((size_t)n * sizeof(int));
    int *best_assign = malloc((size_t)n * sizeof(int));

    for (int restart = 0; restart < n_restarts; restart++) {
        float cr[255], cg[255], cb[255];
        int   ready[255] = {0};
        int   n_ready = 0;

        /* Place all fixed centers first */
        for (int ci = 0; ci < k; ci++) {
            if (fixed[ci]) {
                cr[ci] = (float)seed_r[ci];
                cg[ci] = (float)seed_g[ci];
                cb[ci] = (float)seed_b[ci];
                ready[ci] = 1;
                n_ready++;
            }
        }

        /* Greedily fill unseeded slots.
         * Each restart uses a different first pixel, spreading starting points. */
        for (int ci = 0; ci < k; ci++) {
            if (fixed[ci]) continue;
            if (n_ready == 0) {
                int first = (restart * n / n_restarts) % n;
                cr[ci] = pr[first]; cg[ci] = pg[first]; cb[ci] = pb[first];
            } else {
                long best_d = -1; int best_p = 0;
                for (int p = 0; p < n; p++) {
                    long min_d = LONG_MAX;
                    for (int c = 0; c < k; c++) {
                        if (!ready[c]) continue;
                        long d = rgb_dist2(pr[p], pg[p], pb[p], (int)cr[c], (int)cg[c], (int)cb[c]);
                        if (d < min_d) min_d = d;
                    }
                    if (min_d > best_d) { best_d = min_d; best_p = p; }
                }
                cr[ci] = pr[best_p]; cg[ci] = pg[best_p]; cb[ci] = pb[best_p];
            }
            ready[ci] = 1;
            n_ready++;
        }

        /* K-means iterations */
        for (int p = 0; p < n; p++) assign[p] = -1;
        for (int iter = 0; iter < 50; iter++) {
            int changed = 0;
            for (int p = 0; p < n; p++) {
                long best_d = LONG_MAX; int best_c = 0;
                for (int c = 0; c < k; c++) {
                    long d = rgb_dist2(pr[p], pg[p], pb[p], (int)cr[c], (int)cg[c], (int)cb[c]);
                    if (d < best_d) { best_d = d; best_c = c; }
                }
                if (assign[p] != best_c) { assign[p] = best_c; changed++; }
            }
            if (!changed) break;

            long sum_r[255] = {0}, sum_g[255] = {0}, sum_b[255] = {0};
            int  count[255] = {0};
            for (int p = 0; p < n; p++) {
                int c = assign[p];
                sum_r[c] += pr[p]; sum_g[c] += pg[p]; sum_b[c] += pb[p];
                count[c]++;
            }
            for (int c = 0; c < k; c++) {
                if (fixed[c] || count[c] == 0) continue;
                cr[c] = (float)sum_r[c] / count[c];
                cg[c] = (float)sum_g[c] / count[c];
                cb[c] = (float)sum_b[c] / count[c];
            }
        }

        /* Measure total distortion */
        long distortion = 0;
        for (int p = 0; p < n; p++)
            distortion += rgb_dist2(pr[p], pg[p], pb[p],
                                    (int)(cr[assign[p]] + 0.5f),
                                    (int)(cg[assign[p]] + 0.5f),
                                    (int)(cb[assign[p]] + 0.5f));

        if (distortion < best_distortion) {
            best_distortion = distortion;
            for (int c = 0; c < k; c++) {
                best_r[c] = (uint8_t)(cr[c] + 0.5f);
                best_g[c] = (uint8_t)(cg[c] + 0.5f);
                best_b[c] = (uint8_t)(cb[c] + 0.5f);
            }
            memcpy(best_assign, assign, (size_t)n * sizeof(int));
        }
    }
    free(assign);

    /* Remap pixels using the best result found */
    for (int p = 0; p < n; p++) {
        int i = src_idx[p];
        int c = best_assign[p];
        img[i*4]   = best_r[c];
        img[i*4+1] = best_g[c];
        img[i*4+2] = best_b[c];
    }

    free(best_assign);
    free(pr); free(pg); free(pb); free(src_idx);
}

static int do_encode(const char *in_path, const char *out_path, int target_colors,
                     const char *preview_path, const int *seed_r, const int *seed_g, const int *seed_b,
                     int tile_mode) {
    int w, h, ch;
    unsigned char *img = stbi_load(in_path, &w, &h, &ch, 4);
    if (!img) {
        fprintf(stderr, "failed to load: %s\n", in_path);
        return 1;
    }
    if (w <= 0 || h <= 0 || w > 255 || h > 255) {
        fprintf(stderr, "image dimensions out of range: %dx%d (max 255x255)\n", w, h);
        stbi_image_free(img);
        return 1;
    }

    int total = w * h;

    /* Quantize or snap to built-in palette */
    const BuiltinPalette *builtin = NULL;
    if (target_colors < 0) {
        builtin = get_builtin(-target_colors);
        if (!builtin) {
            fprintf(stderr, "unknown built-in palette %d\n", target_colors);
            stbi_image_free(img);
            return 1;
        }
        printf("snapping to built-in palette %d (%d colors)...\n",
               -target_colors, builtin->n);
        remap_to_palette(img, total, builtin);
    } else if (target_colors > 0) {
        printf("quantizing to %d colors...\n", target_colors);
        quantize(img, total, target_colors, seed_r, seed_g, seed_b);
    }

    /* Stamp all surviving pixels to fully opaque */
    for (int i = 0; i < total; i++)
        if (img[i*4+3] >= 25) img[i*4+3] = 255;

    if (preview_path && target_colors != 0) {
        if (!stbi_write_png(preview_path, w, h, 4, img, w * 4))
            fprintf(stderr, "warning: failed to write preview: %s\n", preview_path);
        else
            printf("preview written to %s\n", preview_path);
    }

    /* Build palette from the (possibly remapped) image.
     * For built-in palettes we use the palette table directly. */
    uint8_t pal_r[127], pal_g[127], pal_b[127];
    int n_colors = 0;

    if (builtin) {
        n_colors = builtin->n;
        for (int ci = 0; ci < n_colors; ci++) {
            pal_r[ci] = builtin->r[ci];
            pal_g[ci] = builtin->g[ci];
            pal_b[ci] = builtin->b[ci];
        }
    } else {
        for (int i = 0; i < total; i++) {
            uint8_t r = img[i*4], g = img[i*4+1], b = img[i*4+2], a = img[i*4+3];
            if (a < 25) continue;
            int found = 0;
            for (int ci = 0; ci < n_colors; ci++) {
                if (pal_r[ci] == r && pal_g[ci] == g && pal_b[ci] == b) { found = 1; break; }
            }
            if (!found) {
                if (n_colors >= 127) {
                    fprintf(stderr, "too many unique colors (max 127)\n");
                    stbi_image_free(img);
                    return 1;
                }
                pal_r[n_colors] = r;
                pal_g[n_colors] = g;
                pal_b[n_colors] = b;
                n_colors++;
            }
        }
    }

    /* Assign indices (transparent pixels get index = n_colors) */
    uint8_t *indices = malloc((size_t)total);
    if (!indices) { stbi_image_free(img); return 1; }

    for (int i = 0; i < total; i++) {
        uint8_t r = img[i*4], g = img[i*4+1], b = img[i*4+2], a = img[i*4+3];
        if (a < 25) {
            indices[i] = (uint8_t)n_colors;
            continue;
        }
        for (int ci = 0; ci < n_colors; ci++) {
            if (pal_r[ci] == r && pal_g[ci] == g && pal_b[ci] == b) {
                indices[i] = (uint8_t)ci;
                break;
            }
        }
    }
    stbi_image_free(img);

    /* Pack indices MSB-first; tile mode skips out-of-bounds pixels */
    int bpp  = bits_needed(n_colors + 1);
    int mask = (1 << bpp) - 1;

    int n_packed = 0;
    if (tile_mode) {
        for (int i = 0; i < total; i++)
            if (tile_in_bounds(i % w, i / w, w, h)) n_packed++;
    } else {
        n_packed = total;
    }
    int data_bytes = (n_packed * bpp + 7) / 8;

    uint8_t *packed = calloc(1, (size_t)data_bytes);
    if (!packed) { free(indices); return 1; }

    int bit_pos = 0;
    for (int i = 0; i < total; i++) {
        if (tile_mode && !tile_in_bounds(i % w, i / w, w, h)) continue;
        int val = indices[i] & mask;
        for (int b = bpp - 1; b >= 0; b--) {
            if ((val >> b) & 1)
                packed[bit_pos / 8] |= (uint8_t)(1 << (7 - (bit_pos % 8)));
            bit_pos++;
        }
    }
    free(indices);

    /* Write file */
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "failed to open output: %s\n", out_path);
        free(packed);
        return 1;
    }
    if (tile_mode) {
        /* cc=0 signals tile-compressed; actual n_colors follows as 4th byte */
        uint8_t hdr[4] = { (uint8_t)w, (uint8_t)h, 0, (uint8_t)n_colors };
        fwrite(hdr, 1, 4, f);
        for (int ci = 0; ci < n_colors; ci++) {
            uint8_t rgb[3] = { pal_r[ci], pal_g[ci], pal_b[ci] };
            fwrite(rgb, 1, 3, f);
        }
    } else {
        int8_t color_count_byte = builtin ? (int8_t)target_colors : (int8_t)n_colors;
        uint8_t hdr[3] = { (uint8_t)w, (uint8_t)h, (uint8_t)color_count_byte };
        fwrite(hdr, 1, 3, f);
        if (!builtin)
            for (int ci = 0; ci < n_colors; ci++) {
                uint8_t rgb[3] = { pal_r[ci], pal_g[ci], pal_b[ci] };
                fwrite(rgb, 1, 3, f);
            }
    }
    fwrite(packed, 1, (size_t)data_bytes, f);
    fclose(f);
    free(packed);

    int hdr_bytes     = tile_mode ? 4 : 3;
    int palette_bytes = (tile_mode || !builtin) ? n_colors * 3 : 0;
    int total_bytes   = hdr_bytes + palette_bytes + data_bytes;
    printf("encoded %s -> %s  [%dx%d, %d colors, %d bpp, %d bytes%s]\n",
           in_path, out_path, w, h, n_colors, bpp, total_bytes,
           tile_mode ? ", tile-compressed" : "");
    printf("palette:\n");
    for (int ci = 0; ci < n_colors; ci++)
        printf("  %d: #%02x%02x%02x\n", ci, pal_r[ci], pal_g[ci], pal_b[ci]);
    return 0;
}

static int do_decode(const char *in_path, const char *out_path) {
    FILE *f = fopen(in_path, "rb");
    if (!f) {
        fprintf(stderr, "failed to open: %s\n", in_path);
        return 1;
    }

    uint8_t header[3];
    if (fread(header, 1, 3, f) != 3) {
        fprintf(stderr, "failed to read header\n");
        fclose(f);
        return 1;
    }
    int w                = header[0];
    int h                = header[1];
    int8_t color_count_s = (int8_t)header[2];

    uint8_t pal_r[127], pal_g[127], pal_b[127];
    int n_colors;

    if (color_count_s < 0) {
        /* Built-in palette — no palette bytes in file */
        const BuiltinPalette *pal = get_builtin((int)(-color_count_s));
        if (!pal) {
            fprintf(stderr, "unknown built-in palette %d\n", (int)(-color_count_s));
            fclose(f);
            return 1;
        }
        n_colors = pal->n;
        for (int ci = 0; ci < n_colors; ci++) {
            pal_r[ci] = pal->r[ci]; pal_g[ci] = pal->g[ci]; pal_b[ci] = pal->b[ci];
        }
    } else {
        n_colors = (int)color_count_s;
        for (int ci = 0; ci < n_colors; ci++) {
            uint8_t rgb[3];
            if (fread(rgb, 1, 3, f) != 3) {
                fprintf(stderr, "truncated palette\n");
                fclose(f);
                return 1;
            }
            pal_r[ci] = rgb[0];
            pal_g[ci] = rgb[1];
            pal_b[ci] = rgb[2];
        }
    }

    int total      = w * h;
    int bpp        = bits_needed(n_colors + 1);
    int mask       = (1 << bpp) - 1;
    int data_bytes = (total * bpp + 7) / 8;

    uint8_t *packed = malloc((size_t)data_bytes);
    if (!packed) { fclose(f); return 1; }
    if ((int)fread(packed, 1, (size_t)data_bytes, f) != data_bytes) {
        fprintf(stderr, "truncated pixel data\n");
        free(packed); fclose(f);
        return 1;
    }
    fclose(f);

    uint8_t *rgba = malloc((size_t)total * 4);
    if (!rgba) { free(packed); return 1; }

    int bit_pos = 0;
    for (int i = 0; i < total; i++) {
        int val = 0;
        for (int b = bpp - 1; b >= 0; b--) {
            if ((packed[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1)
                val |= (1 << b);
            bit_pos++;
        }
        val &= mask;

        uint8_t *px = rgba + i * 4;
        if (val == n_colors) {
            px[0] = px[1] = px[2] = px[3] = 0; /* fully transparent */
        } else if (val < n_colors) {
            px[0] = pal_r[val];
            px[1] = pal_g[val];
            px[2] = pal_b[val];
            px[3] = 255;
        } else {
            px[0] = 255; px[1] = 0; px[2] = 255; px[3] = 255; /* magenta = bad index */
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
    if (argc == 4 && strcmp(argv[1], "-d") == 0)
        return do_decode(argv[2], argv[3]);

    /* Check for -t (tile-compress) flag as first argument */
    int tile_mode = 0;
    if (argc >= 2 && strcmp(argv[1], "-t") == 0) {
        tile_mode = 1;
        argv++; argc--;  /* shift past -t */
    }

    if (argc == 3 && argv[1][0] != '-')
        return do_encode(argv[1], argv[2], 0, NULL, NULL, NULL, NULL, tile_mode);
    if (argc >= 4 && argv[1][0] != '-') {
        int k = atoi(argv[3]);
        if (k == 0 || k < -N_BUILTIN_PALETTES || k > 127) {
            fprintf(stderr, "N must be 1..127 (k-means) or -1..-%d (built-in palette)\n",
                    N_BUILTIN_PALETTES);
            return 1;
        }

        const char *preview = NULL;
        int seed_r[127], seed_g[127], seed_b[127];
        for (int i = 0; i < 127; i++) seed_r[i] = seed_g[i] = seed_b[i] = -1;

        for (int i = 4; i < argc; i++) {
            int idx; unsigned int sr, sg, sb;
            if (sscanf(argv[i], "%d:%02x%02x%02x", &idx, &sr, &sg, &sb) == 4) {
                if (idx < 0 || idx >= k) {
                    fprintf(stderr, "seed index %d out of range (0..%d)\n", idx, k-1);
                    return 1;
                }
                seed_r[idx] = (int)sr;
                seed_g[idx] = (int)sg;
                seed_b[idx] = (int)sb;
            } else if (!preview) {
                preview = argv[i];
            } else {
                fprintf(stderr, "unexpected argument: %s\n", argv[i]);
                return 1;
            }
        }
        return do_encode(argv[1], argv[2], k, preview, seed_r, seed_g, seed_b, tile_mode);
    }

    fprintf(stderr,
        "usage:\n"
        "  encode:          img_conv <in.png> <out.bin>\n"
        "  encode+tile:     img_conv -t <in.png> <out.til>  (skips transparent corners)\n"
        "  encode+quantize: img_conv [-t] <in.png> <out> <N> [preview.png] [idx:rrggbb ...]\n"
        "  encode+built-in: img_conv <in.png> <out.bin> <-N> [preview.png]\n"
        "  decode:          img_conv -d <in.bin/til> <out.png>\n"
        "built-in palettes: -1=Gameboy  -2=Grayscale4  -3=CGA\n");
    return 1;
}
