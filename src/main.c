#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pak.h"
#include "player.h"
#include "items.h"
#include "gfx.h"
#include "game.h"
#include "enemies.h"
#include "combat.h"
#include "town.h"
#include "world.h"
#include "skills.h"

/* State machine */
GameState state = STATE_WORLD;

/* --- Input --- */

static int g_pendingKey = 0;

static void handleMenuInput(int key) {
    switch (key) {
        case VK_UP:
            inventory.selected--;
            if (inventory.selected < 0)
                inventory.selected = inventory.count > 0 ? inventory.count - 1 : 0;
            break;
        case VK_DOWN:
            if (inventory.count > 0)
                inventory.selected = (inventory.selected + 1) % inventory.count;
            break;
        case VK_RETURN:
            if (inventory.count > 0)
                useOrEquipItem(inventory.selected);
            break;
        case VK_ESCAPE:
            state = STATE_WORLD;
            break;
    }
}

static void handleDungeonInput(int key) { if (key == VK_ESCAPE) state = STATE_WORLD; }

/* --- Render --- */

static void renderMenu(void) {
    int x = 60, y = 55;
    const int lineH = 22;
    char buf[48];
    int i;

    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(10, 20, 50));
    drawText(x, y, "INVENTORY", rgb(220, 220, 255), 2);
    y += lineH + 4;

    if (inventory.count == 0) {
        drawText(x, y, "  (empty)", rgb(120, 120, 120), 2);
    } else {
        for (i = 0; i < inventory.count; i++) {
            uint8_t id      = inventory.items[i];
            int     sel     = (i == inventory.selected);
            uint32_t color  = sel ? rgb(255, 255, 100) : rgb(180, 180, 180);

            snprintf(buf, sizeof(buf), "%s%-10s",
                sel ? "> " : "  ",
                itemName(id));

            /* Show (E) for currently equipped items */
            if (id == player.weaponId || id == player.armorId) {
                size_t len = strlen(buf);
                buf[len]   = ' ';
                buf[len+1] = '(';
                buf[len+2] = 'E';
                buf[len+3] = ')';
                buf[len+4] = '\0';
            }

            drawText(x, y, buf, color, 2);
            y += lineH;
        }
    }

    /* Divider */
    int divY = y + 6;
    fillRect(x, divY, gfxWidth - 120, 1, rgb(60, 60, 100));
    y = divY + 10;

    /* Item info for selected */
    if (inventory.count > 0) {
        uint8_t id = inventory.items[inventory.selected];
        int preAtk, preDef, preHp;
        getPreviewStats(id, &preAtk, &preDef, &preHp);

        drawText(x, y, itemDesc(id), rgb(160, 200, 255), 2);
        y += lineH + 2;

        int curAtk = getAttack();
        int curDef = getDefense();

        if (preAtk != curAtk)
            snprintf(buf, sizeof(buf), "ATK: %d -> %d", curAtk, preAtk);
        else
            snprintf(buf, sizeof(buf), "ATK: %d", curAtk);
        drawText(x, y, buf, rgb(220, 100, 100), 2);
        y += lineH;

        if (preDef != curDef)
            snprintf(buf, sizeof(buf), "DEF: %d -> %d", curDef, preDef);
        else
            snprintf(buf, sizeof(buf), "DEF: %d", curDef);
        drawText(x, y, buf, rgb(100, 160, 220), 2);
        y += lineH;

        if (preHp != playerHp)
            snprintf(buf, sizeof(buf), "HP:  %d -> %d / %d", playerHp, preHp, player.maxHp);
        else
            snprintf(buf, sizeof(buf), "HP:  %d / %d", playerHp, player.maxHp);
        drawText(x, y, buf, rgb(100, 220, 100), 2);
        y += lineH + 4;

        snprintf(buf, sizeof(buf), "LVL: %d", player.level);
        drawText(x, y, buf, rgb(220, 200, 100), 2); y += lineH;
        snprintf(buf, sizeof(buf), "XP:  %d / %d", player.xp, xpToNext());
        drawText(x, y, buf, rgb(180, 160, 80), 2);
    }
}

static void renderDungeon(void) {
    /* Placeholder — dungeon system coming soon */
    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(80, 0, 80));
}

/* --- Win32 --- */

static int  g_running = 1;
static HWND g_hwnd;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_KEYDOWN:
            g_pendingKey = (int)wp;
            return 0;
        case WM_SIZE:
            gfxResize((int)LOWORD(lp), (int)HIWORD(lp));
            worldUpdateCamera();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            g_running = 0;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR cmdLine, int nCmdShow) {
    (void)hPrev; (void)cmdLine;

    srand((unsigned int)GetTickCount());

    if (!pakOpen("data.pak")) return 1;

    PakData playerData  = pakRead("assets/player.dat");
    PakData itemData    = pakRead("assets/items.dat");
    PakData enemyData   = pakRead("assets/enemies.dat");
    PakData dialogData  = pakRead("assets/dialog.dat");

    if (!loadPlayer(playerData)) { pakClose(); return 1; }
    loadItems(itemData);     /* optional — falls back to builtins if not in pak */
    loadEnemies(enemyData);  /* optional — falls back to builtins if not in pak */
    loadDialogs(dialogData); /* optional — falls back to builtins if not in pak */
    free(playerData.data);
    free(itemData.data);
    free(enemyData.data);
    free(dialogData.data);

    const int screenW = 640;
    const int screenH = 480;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "TinyHero";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    RECT wr = {0, 0, screenW, screenH};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowA(
        "TinyHero", "Tiny Hero",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL
    );

    gfxInit(g_hwnd, screenW, screenH);

    if (!worldLoadNamed("assets/map1.bin")) { pakClose(); gfxShutdown(); return 1; }
    playerHp = player.maxHp;
    /* pak stays open for dynamic map loading during gameplay */

    ShowWindow(g_hwnd, nCmdShow);

    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_pendingKey) {
            /* P is a global hotkey — opens skills from world, closes from skills */
            if (g_pendingKey == 'P' && (state == STATE_WORLD || state == STATE_SKILLS)) {
                state = (state == STATE_SKILLS) ? STATE_WORLD : STATE_SKILLS;
            } else {
                switch (state) {
                    case STATE_WORLD:   handleWorldInput(g_pendingKey);   break;
                    case STATE_COMBAT:  handleCombatInput(g_pendingKey);  break;
                    case STATE_MENU:    handleMenuInput(g_pendingKey);    break;
                    case STATE_SKILLS:  handleSkillsInput(g_pendingKey);  break;
                    case STATE_DIALOG:  handleDialogInput(g_pendingKey);  break;
                    case STATE_DUNGEON: handleDungeonInput(g_pendingKey); break;
                    case STATE_TOWN:    handleTownInput(g_pendingKey);    break;
                }
            }
            g_pendingKey = 0;
        }

        clearScreen();
        switch (state) {
            case STATE_WORLD:   renderWorld();   break;
            case STATE_COMBAT:  renderCombat();  break;
            case STATE_MENU:    renderMenu();    break;
            case STATE_SKILLS:  renderSkills();  break;
            case STATE_DIALOG:  renderDialog();  break;
            case STATE_DUNGEON: renderDungeon(); break;
            case STATE_TOWN:    renderTown();    break;
        }

        gfxPresent(g_hwnd);
        Sleep(16);
    }

    pakClose();
    gfxShutdown();
    return 0;
}
