#include "gfx.h"
#include "font8x8_basic.h"
#include <string.h>

int gfxWidth  = 0;
int gfxHeight = 0;

static HDC       g_backDC;
static HBITMAP   g_backBmp;
static uint32_t* g_pixels;

void gfxInit(HWND hwnd, int w, int h) {
    gfxWidth  = w;
    gfxHeight = h;

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
    BitBlt(hdc, 0, 0, gfxWidth, gfxHeight, g_backDC, 0, 0, SRCCOPY);
    ReleaseDC(hwnd, hdc);
}

void gfxResize(int w, int h) {
    if (w <= 0 || h <= 0 || !g_backDC) return;
    SelectObject(g_backDC, (HGDIOBJ)GetStockObject(NULL_BRUSH));
    DeleteObject(g_backBmp);

    gfxWidth  = w;
    gfxHeight = h;

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

void drawBW(const uint8_t *data, uint32_t size, uint32_t color) {
    if (!data || size < 4) return;
    int imgW = (int)(data[0] | (data[1] << 8));
    int imgH = (int)(data[2] | (data[3] << 8));
    if (imgW <= 0 || imgH <= 0) return;

    int total = imgW * imgH;
    uint8_t *bits = malloc(total);
    if (!bits) return;

    /* decode RLE into flat bitmap */
    int pixel = 0;
    uint8_t bit = 0;
    uint32_t pos = 4;
    while (pos < size && pixel < total) {
        int run = data[pos++];
        for (int i = 0; i < run && pixel < total; i++)
            bits[pixel++] = bit;
        bit ^= 1;
    }

    /* letterbox: largest fit preserving aspect ratio */
    int drawW, drawH;
    if (gfxWidth * imgH > gfxHeight * imgW) {
        drawH = gfxHeight;
        drawW = imgW * gfxHeight / imgH;
    } else {
        drawW = gfxWidth;
        drawH = imgH * gfxWidth / imgW;
    }
    int offX = (gfxWidth  - drawW) / 2;
    int offY = (gfxHeight - drawH) / 2;

    for (int py = 0; py < drawH; py++) {
        int sy = py * imgH / drawH;
        for (int px = 0; px < drawW; px++) {
            int sx = px * imgW / drawW;
            g_pixels[(offY + py) * gfxWidth + (offX + px)] = bits[sy * imgW + sx] ? color : 0xFFFFFF;
        }
    }

    free(bits);
}

void drawText(int x, int y, const char* text, uint32_t color, int scale) {
    for (int i = 0; text[i]; i++)
        drawChar(x + i * 8 * scale, y, text[i], color, scale);
}
