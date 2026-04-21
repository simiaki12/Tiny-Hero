#include "gfx.h"
#include "font8x8_basic.h"
#include <string.h>

int gfxWidth  = 0;
int gfxHeight = 0;

static HDC       g_backDC;
static HBITMAP   g_backBmp;
static uint32_t* g_pixels;
static int       g_winW = 0;
static int       g_winH = 0;

void gfxInit(HWND hwnd, int w, int h) {
    gfxWidth  = w;
    gfxHeight = h;
    g_winW    = w;
    g_winH    = h;

    HDC hdc  = GetDC(hwnd);
    g_backDC = CreateCompatibleDC(hdc);
    ReleaseDC(hwnd, hdc);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_backBmp = CreateDIBSection(g_backDC, &bmi, DIB_RGB_COLORS, (void**)&g_pixels, NULL, 0);
    SelectObject(g_backDC, g_backBmp);
}

void gfxPresent(HWND hwnd) {
    HDC hdc = GetDC(hwnd);

    /* Letterbox: scale logical buffer into window preserving aspect ratio */
    int dstW, dstH;
    if (g_winW * gfxHeight > g_winH * gfxWidth) {
        dstH = g_winH;
        dstW = gfxWidth * g_winH / gfxHeight;
    } else {
        dstW = g_winW;
        dstH = gfxHeight * g_winW / gfxWidth;
    }
    int offX = (g_winW - dstW) / 2;
    int offY = (g_winH - dstH) / 2;

    /* Fill black bars */
    HBRUSH black = GetStockObject(BLACK_BRUSH);
    if (offX > 0) {
        RECT r = {0, 0, offX, g_winH};             FillRect(hdc, &r, black);
        r = (RECT){offX + dstW, 0, g_winW, g_winH}; FillRect(hdc, &r, black);
    }
    if (offY > 0) {
        RECT r = {0, 0, g_winW, offY};              FillRect(hdc, &r, black);
        r = (RECT){0, offY + dstH, g_winW, g_winH}; FillRect(hdc, &r, black);
    }

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, offX, offY, dstW, dstH, g_backDC, 0, 0, gfxWidth, gfxHeight, SRCCOPY);
    ReleaseDC(hwnd, hdc);
}

void gfxResize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    g_winW = w;
    g_winH = h;
    /* Backbuffer stays at fixed logical size — no recreate needed */
}

void gfxShutdown(void) {
    DeleteObject(g_backBmp);
    DeleteDC(g_backDC);
}

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void fillRect(int x, int y, int w, int h, uint32_t color) {
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w) > gfxWidth  ? gfxWidth  : (x + w);
    int y2 = (y + h) > gfxHeight ? gfxHeight : (y + h);
    for (int py = y1; py < y2; py++)
        for (int px = x1; px < x2; px++)
            g_pixels[py * gfxWidth + px] = color;
}

void clearScreen(void) {
    memset(g_pixels, 0, (size_t)gfxWidth * gfxHeight * 4);
}

static void drawChar(int x, int y, char c, uint32_t color, int scale) {
    if ((unsigned char)c >= 128) return;
    const unsigned char* glyph = font8x8_basic[(unsigned char)c];
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            if ((glyph[row] >> col) & 1)
                fillRect(x + col * scale, y + row * scale, scale, scale, color);
}

void drawSprite8(int dx, int dy, const uint8_t* data, const uint32_t* pal, int scale) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            uint8_t idx = data[row * 8 + col];
            if (idx == 0) continue;
            uint32_t color = pal[idx];
            int px = dx + col * scale;
            int py = dy + row * scale;
            int x1 = px < 0 ? 0 : px;
            int y1 = py < 0 ? 0 : py;
            int x2 = px + scale > gfxWidth  ? gfxWidth  : px + scale;
            int y2 = py + scale > gfxHeight ? gfxHeight : py + scale;
            for (int sy = y1; sy < y2; sy++)
                for (int sx = x1; sx < x2; sx++)
                    g_pixels[sy * gfxWidth + sx] = color;
        }
    }
}

/* Built-in palettes matching tools/img_conv.c */
static const uint8_t g_builtin_pal[][16][3] = {
    /* 1 — Gameboy DMG */
    { {15,56,15},{48,98,48},{139,172,15},{155,188,15} },
    /* 2 — 4-shade grayscale */
    { {0,0,0},{85,85,85},{170,170,170},{255,255,255} },
    /* 3 — CGA */
    { {0,0,0},{0,0,170},{0,170,0},{0,170,170},
      {170,0,0},{170,0,170},{170,170,0},{170,170,170},
      {85,85,85},{85,85,255},{85,255,85},{85,255,255},
      {255,85,85},{255,85,255},{255,255,85},{255,255,255} },
};
static const int g_builtin_pal_size[] = { 4, 4, 16 };
#define N_BUILTIN_PAL 3

static int bin_bits_needed(int n) {
    int b = 1;
    while ((1 << b) < n) b++;
    return b;
}

void drawBin(int x, int y, const uint8_t *data, int scale) {
    if (!data) return;
    int w            = data[0];
    int h            = data[1];
    int8_t cc        = (int8_t)data[2];
    int n_colors;
    uint32_t pal[127];

    const uint8_t *src = data + 3;

    if (cc < 0) {
        /* Built-in palette */
        int id = (int)(-cc) - 1;
        if (id < 0 || id >= N_BUILTIN_PAL) return;
        n_colors = g_builtin_pal_size[id];
        for (int ci = 0; ci < n_colors; ci++)
            pal[ci] = rgb(g_builtin_pal[id][ci][0],
                          g_builtin_pal[id][ci][1],
                          g_builtin_pal[id][ci][2]);
    } else {
        /* Inline palette */
        n_colors = (int)cc;
        for (int ci = 0; ci < n_colors; ci++, src += 3)
            pal[ci] = rgb(src[0], src[1], src[2]);
    }

    int bpp  = bin_bits_needed(n_colors + 1);
    int mask = (1 << bpp) - 1;

    int bit_pos = 0;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            /* Unpack next index MSB-first */
            int val = 0;
            for (int b = bpp - 1; b >= 0; b--) {
                if ((src[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1)
                    val |= (1 << b);
                bit_pos++;
            }
            val &= mask;
            if (val == n_colors) continue; /* transparent */

            int px = x + col * scale;
            int py = y + row * scale;
            int x1 = px < 0 ? 0 : px;
            int y1 = py < 0 ? 0 : py;
            int x2 = px + scale > gfxWidth  ? gfxWidth  : px + scale;
            int y2 = py + scale > gfxHeight ? gfxHeight : py + scale;
            uint32_t color = pal[val];
            for (int sy = y1; sy < y2; sy++)
                for (int sx = x1; sx < x2; sx++)
                    g_pixels[sy * gfxWidth + sx] = color;
        }
    }
}

void drawBWAt(int x, int y, int dstW, int dstH,
              const uint8_t *data, uint32_t size, uint32_t color, uint32_t color2) {
    if (!data || size < 4) return;
    int imgW = (int)(data[0] | (data[1] << 8));
    int imgH = (int)(data[2] | (data[3] << 8));
    if (imgW <= 0 || imgH <= 0 || dstW <= 0 || dstH <= 0) return;

    int total = imgW * imgH;
    uint8_t *bits = malloc(total);
    if (!bits) return;

    int pixel = 0;
    uint8_t bit = 0;
    uint32_t pos = 4;
    while (pos < size && pixel < total) {
        int run = data[pos++];
        for (int i = 0; i < run && pixel < total; i++)
            bits[pixel++] = bit;
        bit ^= 1;
    }

    for (int py = 0; py < dstH; py++) {
        int gy = y + py;
        if (gy < 0 || gy >= gfxHeight) continue;
        int sy = py * imgH / dstH;
        for (int px = 0; px < dstW; px++) {
            int gx = x + px;
            if (gx < 0 || gx >= gfxWidth) continue;
            int sx = px * imgW / dstW;
            g_pixels[gy * gfxWidth + gx] = bits[sy * imgW + sx] ? color : color2;
        }
    }

    free(bits);
}

void drawBW(const uint8_t *data, uint32_t size, uint32_t color) {
    if (!data || size < 4) return;
    int imgW = (int)(data[0] | (data[1] << 8));
    int imgH = (int)(data[2] | (data[3] << 8));
    if (imgW <= 0 || imgH <= 0) return;

    /* Letterbox: largest fit preserving aspect ratio */
    int dstW, dstH;
    if (gfxWidth * imgH > gfxHeight * imgW) {
        dstH = gfxHeight;
        dstW = imgW * gfxHeight / imgH;
    } else {
        dstW = gfxWidth;
        dstH = imgH * gfxWidth / imgW;
    }
    int offX = (gfxWidth  - dstW) / 2;
    int offY = (gfxHeight - dstH) / 2;

    drawBWAt(offX, offY, dstW, dstH, data, size, color,0xFFFFFF);
}

void drawText(int x, int y, const char* text, uint32_t color, int scale) {
    for (int i = 0; text[i]; i++)
        drawChar(x + i * 8 * scale, y, text[i], color, scale);
}
