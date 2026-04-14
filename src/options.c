#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "gfx.h"
#include "game.h"
#include "audio.h"
#include "options.h"

static GameState g_returnState;
static int       g_vol;

void enterOptions(GameState from) {
    g_returnState = from;
    g_vol         = audioGetVolume();
    state         = STATE_OPTIONS;
}

void handleOptionsInput(int key) {
    switch (key) {
        case VK_LEFT:
            if (g_vol > 0) { g_vol--; audioSetVolume(g_vol); }
            break;
        case VK_RIGHT:
            if (g_vol < 10) { g_vol++; audioSetVolume(g_vol); }
            break;
        case VK_ESCAPE:
        case VK_RETURN:
            state = g_returnState;
            break;
    }
}

void renderOptions(void) {
    const int cx = gfxWidth  / 2;
    const int cy = gfxHeight / 2;

    fillRect(cx - 120, cy - 80, 240, 160, rgb(10, 10, 30));
    fillRect(cx - 120, cy - 80, 240,   1, rgb(80, 80, 160));
    fillRect(cx - 120, cy + 79, 240,   1, rgb(80, 80, 160));

    drawText(cx - 56, cy - 60, "OPTIONS", rgb(220, 220, 255), 2);

    drawText(cx - 80, cy - 10, "Volume", rgb(180, 180, 180), 2);

    /* Slider bar */
    const int barX = cx - 80;
    const int barY = cy + 16;
    const int barW = 160;
    const int barH = 12;
    fillRect(barX, barY, barW, barH, rgb(40, 40, 80));
    if (g_vol > 0)
        fillRect(barX, barY, g_vol * barW / 10, barH, rgb(100, 160, 255));

    char buf[8];
    snprintf(buf, sizeof(buf), "%d/10", g_vol);
    drawText(cx + 90, barY - 2, buf, rgb(220, 220, 100), 2);

    drawText(cx - 104, cy + 50, "< > to adjust  ESC to back", rgb(80, 80, 80), 1);
}
