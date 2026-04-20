#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "gfx.h"
#include "game.h"
#include "save.h"
#include "pausemenu.h"
#include "options.h"

#define PAUSE_ITEMS 4
static const char *g_labels[PAUSE_ITEMS] = {
    "Resume", "Save", "Options", "Exit to Menu"
};

static GameState g_returnState;
static int       g_sel      = 0;
static int       g_inForm   = 0;

/* Save name form */
#define NAME_MAX 32
static char g_name[NAME_MAX + 1];
static int  g_nameLen = 0;

void enterPauseMenu(GameState from) {
    g_returnState = from;
    g_sel         = 0;
    g_inForm      = 0;
    g_nameLen     = 0;
    g_name[0]     = '\0';
    state         = STATE_PAUSE_MENU;
}

static void handleFormInput(int key, char ch) {
    if (key == VK_ESCAPE) {
        g_inForm = 0;
        return;
    }
    if (key == VK_RETURN) {
        if (g_nameLen > 0 && saveGameAs(g_name))
            g_inForm = 0;
        return;
    }
    if (key == VK_BACK) {
        if (g_nameLen > 0) g_name[--g_nameLen] = '\0';
        return;
    }
    /* Printable ASCII from WM_CHAR */
    if (ch >= 0x20 && ch < 0x7F && g_nameLen < NAME_MAX) {
        g_name[g_nameLen++] = ch;
        g_name[g_nameLen]   = '\0';
    }
}

void handlePauseMenuInput(int key, char ch) {
    if (g_inForm) { handleFormInput(key, ch); return; }

    switch (key) {
        case VK_UP:
            g_sel = (g_sel + PAUSE_ITEMS - 1) % PAUSE_ITEMS;
            break;
        case VK_DOWN:
            g_sel = (g_sel + 1) % PAUSE_ITEMS;
            break;
        case VK_RETURN:
            switch (g_sel) {
                case 0: state = g_returnState; break;           /* Resume */
                case 1: g_inForm = 1; g_nameLen = 0; g_name[0] = '\0'; break; /* Save */
                case 2: enterOptions(STATE_PAUSE_MENU); break;  /* Options */
                case 3: state = STATE_MAIN_MENU; break;         /* Exit to menu */
            }
            break;
        case VK_ESCAPE:
            state = g_returnState;
            break;
    }
}

void renderPauseMenu(void) {
    const int cx     = gfxWidth  / 2;
    const int lineH  = 24;

    /* Dim overlay */
    for (int y = 0; y < gfxHeight; y += 2)
        for (int x = 0; x < gfxWidth; x++)
            /* every other row darkened — simple 50% overlay without alpha */
            /* fill handled by clearScreen + render already, just draw a semi-transparent box */
            (void)x;
    fillRect(cx - 120, gfxHeight / 2 - 80, 240, 160, rgb(10, 10, 30));
    fillRect(cx - 120, gfxHeight / 2 - 80, 240, 1,   rgb(80, 80, 160));
    fillRect(cx - 120, gfxHeight / 2 + 79, 240, 1,   rgb(80, 80, 160));

    if (g_inForm) {
        int fy = gfxHeight / 2 - 30;
        drawText(cx - 80, fy, "Save name:", rgb(220, 220, 255), 2);
        fy += 28;

        /* Input box */
        fillRect(cx - 112, fy - 2, 224, 20, rgb(30, 30, 60));
        char display[NAME_MAX + 2];
        snprintf(display, sizeof(display), "%s_", g_name);
        drawText(cx - 104, fy, display, rgb(255, 255, 100), 1);

        drawText(cx - 120, fy + 26, "Enter to confirm  Esc to cancel", rgb(80, 80, 80), 1);
        return;
    }

    const int startY = gfxHeight / 2 - (PAUSE_ITEMS * lineH) / 2;
    drawText(cx - 40, startY - 28, "PAUSED", rgb(220, 220, 255), 2);

    for (int i = 0; i < PAUSE_ITEMS; i++) {
        int sel = (i == g_sel);
        uint32_t color = sel ? rgb(255, 255, 100) : rgb(180, 180, 180);
        char buf[40];
        snprintf(buf, sizeof(buf), "%s%s", sel ? "> " : "  ", g_labels[i]);
        int textW = (int)(strlen(buf) * 8 * 2);
        drawText(cx - textW / 2, startY + i * lineH, buf, color, 2);
    }
}
