#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

extern int gfxWidth, gfxHeight;

void     gfxInit(HWND hwnd, int w, int h);
void     gfxResize(int w, int h);
void     gfxPresent(HWND hwnd);
void     gfxShutdown(void);

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
void     fillRect(int x, int y, int w, int h, uint32_t color);
void     clearScreen(void);
void     drawText(int x, int y, const char* text, uint32_t color, int scale);
void     drawSprite8(int x, int y, const uint8_t* data, const uint32_t* pal, int scale);
