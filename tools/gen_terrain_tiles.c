/*
 * gen_terrain_tiles — outputs hills.png and mountains.png
 * matching the isometric tile format used by img_conv -t
 *
 * Build:  gcc -Os tools/gen_terrain_tiles.c -o build/gen_terrain_tiles
 * Run:    ./build/gen_terrain_tiles
 */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <string.h>
#include <stdint.h>

#define W 32

/* ── pixel helpers ─────────────────────────────────────────────────── */

typedef struct { uint8_t r, g, b; } RGB;

static uint8_t img[W * 64 * 4];

static void put(int x, int y, RGB c) {
    uint8_t *p = &img[(y * W + x) * 4];
    p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = 255;
}

/* ── isometric shape helpers ─────────────────────────────────────────*/

static int in_diamond(int x, int y) {
    int r = y + 1;
    if (r < 1 || r >= 16) return 0;
    int span = (r <= 8 ? r : 16 - r) * 4;
    return x >= 16 - span / 2 && x < 16 + span / 2;
}

static int in_left_wall(int x, int y, int h) {
    int wh = h - 16;
    if (wh <= 0 || x >= 16) return 0;
    int ty = 8 + x / 2;
    return y >= ty && y < ty + wh;
}

static int in_right_wall(int x, int y, int h) {
    int wh = h - 16;
    if (wh <= 0 || x < 16) return 0;
    int col = x - 16;
    int ty = 15 - col / 2;
    return y >= ty && y < ty + wh;
}

/* ── hills (32x24, 3 colours) ────────────────────────────────────────
 *   Top face: two greens (light upper / medium lower), simulating a
 *   rounded mound lit from above-left.
 *   Wall face: darker green shadow.
 */
static void gen_hills(void) {
    const int H = 24;
    memset(img, 0, W * H * 4);

    RGB hi   = {120, 185,  60};   /* sunlit top     */
    RGB mid  = { 80, 140,  35};   /* main surface   */
    RGB wall_l = { 62, 110,  26}; /* left wall (lit)*/
    RGB wall_r = { 42,  78,  16}; /* right wall (shadow) */

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (in_diamond(x, y)) {
                /* Top 8 rows of diamond = highlight, bottom = mid */
                put(x, y, y < 8 ? hi : mid);
            } else if (in_left_wall(x, y, H)) {
                put(x, y, wall_l);
            } else if (in_right_wall(x, y, H)) {
                put(x, y, wall_r);
            }
        }
    }

    /* Thin dark edge along the bottom of the diamond face to separate
     * top from wall — one row at each outer column's transition point */
    for (int x = 0; x < W; x++) {
        int edge_y;
        if (x < 16) { edge_y = 8 + x / 2; }
        else         { int c = x - 16; edge_y = 15 - c / 2; }
        if (edge_y >= 0 && edge_y < 16 && in_diamond(x, edge_y))
            put(x, edge_y, mid);
    }

    stbi_write_png("assets/pngs/hills.png", W, H, 4, img, W * 4);
    printf("wrote assets/pngs/hills.png  (%dx%d, 3 colours)\n", W, H);
}

/* ── mountains (32x48, 4 colours) ───────────────────────────────────
 *   Diamond top: snow cap (rows 0-3), then rock (rows 4-14).
 *   Tall wall: left face medium rock, right face dark shadow.
 *   A diagonal highlight line on the left wall simulates a cliff edge.
 */
static void gen_mountains(void) {
    const int H = 48;
    memset(img, 0, W * H * 4);

    RGB snow   = {220, 215, 200};  /* snow cap            */
    RGB rock_l = {130, 125, 115};  /* lit rock face       */
    RGB rock_m = { 82,  78,  70};  /* mid rock / left wall*/
    RGB rock_d = { 42,  40,  36};  /* shadow / right wall */

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (in_diamond(x, y)) {
                if (y < 4)       put(x, y, snow);
                else if (y < 10) put(x, y, rock_l);
                else             put(x, y, rock_m);
            } else if (in_left_wall(x, y, H)) {
                /* Diagonal highlight: one column wide, traces the left edge */
                int edge_x = (y - 8) * 2;   /* x where left-wall top is */
                put(x, y, (x == edge_x || x == edge_x + 1) ? rock_l : rock_m);
            } else if (in_right_wall(x, y, H)) {
                put(x, y, rock_d);
            }
        }
    }

    stbi_write_png("assets/pngs/mountains.png", W, H, 4, img, W * 4);
    printf("wrote assets/pngs/mountains.png  (%dx%d, 4 colours)\n", W, H);
}

int main(void) {
    gen_hills();
    gen_mountains();
    return 0;
}
