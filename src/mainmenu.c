#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "gfx.h"
#include "game.h"
#include "save.h"
#include "mainmenu.h"

/* --- Top-level menu --- */

static int g_sel = 0;
#define MENU_ITEMS 4
static const char *g_labels[MENU_ITEMS] = {
    "New Game", "Load Game", "Options", "Exit"
};

/* --- Save browser --- */

static int  g_inBrowser  = 0;
static int  g_browserSel = 0;
static int  g_saveCount  = 0;
static char g_saveNames[MAX_SAVES][64];

static void enterBrowser(void) {
    g_saveCount  = listSaves(g_saveNames, MAX_SAVES);
    g_browserSel = 0;
    g_inBrowser  = 1;
}

static void handleBrowserInput(int key) {
    switch (key) {
        case VK_UP:
            if (g_browserSel > 0) g_browserSel--;
            break;
        case VK_DOWN:
            if (g_browserSel < g_saveCount - 1) g_browserSel++;
            break;
        case VK_RETURN:
            if (g_saveCount > 0 && loadGameNamed(g_saveNames[g_browserSel]))
                state = STATE_WORLD;
            break;
        case VK_ESCAPE:
            g_inBrowser = 0;
            break;
    }
}

static void renderBrowser(void) {
    const int cx     = gfxWidth  / 2;
    const int lineH  = 22;
    const int startY = gfxHeight / 2 - (g_saveCount * lineH) / 2;

    fillRect(0, 0, gfxWidth, gfxHeight, rgb(0, 0, 0));
    drawText(cx - 72, startY - 40, "LOAD GAME", rgb(220, 220, 255), 2);

    if (g_saveCount == 0) {
        drawText(cx - 80, gfxHeight / 2, "No saves found.", rgb(120, 120, 120), 2);
        return;
    }

    for (int i = 0; i < g_saveCount; i++) {
        int sel = (i == g_browserSel);
        uint32_t color = sel ? rgb(255, 255, 100) : rgb(180, 180, 180);
        char buf[72];
        snprintf(buf, sizeof(buf), "%s%s", sel ? "> " : "  ", g_saveNames[i]);
        int textW = (int)(strlen(buf) * 8 * 2);
        drawText(cx - textW / 2, startY + i * lineH, buf, color, 2);
    }

    drawText(cx - 96, startY + g_saveCount * lineH + 12,
             "ESC to go back", rgb(80, 80, 80), 1);
}

/* --- Public --- */

void handleMainMenuInput(int key) {
    if (g_inBrowser) { handleBrowserInput(key); return; }

    switch (key) {
        case VK_UP:
            g_sel = (g_sel + MENU_ITEMS - 1) % MENU_ITEMS;
            break;
        case VK_DOWN:
            g_sel = (g_sel + 1) % MENU_ITEMS;
            break;
        case VK_RETURN:
            switch (g_sel) {
                case 0: state = STATE_WORLD; break;
                case 1: if (anySaveExists()) enterBrowser(); break;
                case 2: break; /* Options — TODO */
                case 3: PostQuitMessage(0); break;
            }
            break;
    }
}

void renderMainMenu(void) {
    if (g_inBrowser) { renderBrowser(); return; }

    /* TODO: animated warrior GIF behind options */
    const int cx     = gfxWidth  / 2;
    const int lineH  = 24;
    const int startY = gfxHeight / 2 - (MENU_ITEMS * lineH) / 2;

    fillRect(0, 0, gfxWidth, gfxHeight, rgb(0, 0, 0));
    drawText(cx - 72, startY - 48, "TINY HERO", rgb(220, 220, 255), 2);

    int hasSave = anySaveExists();
    for (int i = 0; i < MENU_ITEMS; i++) {
        int sel      = (i == g_sel);
        int disabled = (i == 1 && !hasSave);
        uint32_t color = disabled ? rgb(80, 80, 80) :
                         sel      ? rgb(255, 255, 100) : rgb(180, 180, 180);
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%s", sel ? "> " : "  ", g_labels[i]);
        int textW = (int)(strlen(buf) * 8 * 2);
        drawText(cx - textW / 2, startY + i * lineH, buf, color, 2);
    }
}
