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
#include "combat.h"
#include "town.h"

#define TILE_SIZE     64
#define MAX_MAP_TILES (256 * 256)

/* Map */
static int     mapWidth  = 0;
static int     mapHeight = 0;
static uint8_t mapGfx[MAX_MAP_TILES];
static uint8_t mapLoc[MAX_MAP_TILES];

/* Player position */
static int playerX = 2, playerY = 2;

/* State machine */
GameState state = STATE_WORLD;

typedef enum { LOC_EMPTY = 0, LOC_ENEMY = 1, LOC_TOWN = 2, LOC_DUNGEON = 3 } LocType;

static int loadMap(PakData data) {
    if (data.size < 2) return 0;
    mapWidth  = data.data[0];
    mapHeight = data.data[1];
    int n = mapWidth * mapHeight;
    if ((int)data.size < 2 + n * 2) return 0;
    memcpy(mapGfx, data.data + 2,     n);
    memcpy(mapLoc, data.data + 2 + n, n);
    return 1;
}

/* --- Input --- */

static int g_pendingKey = 0;

static void handleWorldInput(int key) {
    if (key == 'I') { state = STATE_MENU; return; }

    int newX = playerX, newY = playerY;
    switch (key) {
        case VK_UP:    newY--; break;
        case VK_DOWN:  newY++; break;
        case VK_LEFT:  newX--; break;
        case VK_RIGHT: newX++; break;
        default: return;
    }

    if (newX >= 0 && newX < mapWidth &&
        newY >= 0 && newY < mapHeight &&
        mapGfx[newY * mapWidth + newX] == 0) {
        playerX = newX;
        playerY = newY;

        LocType loc = (LocType)mapLoc[newY * mapWidth + newX];
        if (loc == LOC_ENEMY)   startCombat();
        if (loc == LOC_TOWN)    startTown();
        if (loc == LOC_DUNGEON) state = STATE_DUNGEON;
    }
}

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

static void renderWorld(void) {
    for (int y = 0; y < mapHeight; y++) {
        for (int x = 0; x < mapWidth; x++) {
            uint32_t color;
            if (mapGfx[y * mapWidth + x] == 1) {
                color = rgb(100, 100, 100);
            } else {
                switch ((LocType)mapLoc[y * mapWidth + x]) {
                    case LOC_ENEMY:   color = rgb(120,  30,  30); break;
                    case LOC_TOWN:    color = rgb(150, 120,   0); break;
                    case LOC_DUNGEON: color = rgb( 80,   0,  80); break;
                    default:          color = rgb(  0, 100,   0); break;
                }
            }
            fillRect(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, color);
        }
    }
    fillRect(playerX * TILE_SIZE, playerY * TILE_SIZE, TILE_SIZE, TILE_SIZE, rgb(200, 50, 50));
}

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

    if (!pakOpen("data.pak")) return 1;

    PakData mapData    = pakRead("assets/map1.bin");
    PakData playerData = pakRead("assets/player.dat");
    PakData itemData   = pakRead("assets/items.dat");
    pakClose();

    if (!loadMap(mapData) || !loadPlayer(playerData)) return 1;
    loadItems(itemData);  /* optional — falls back to builtins if not in pak */

    free(mapData.data);
    free(playerData.data);
    free(itemData.data);
    playerHp = player.maxHp;

    int screenW = mapWidth  * TILE_SIZE;
    int screenH = mapHeight * TILE_SIZE;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "TinyHero";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    RECT wr = {0, 0, screenW, screenH};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowA(
        "TinyHero", "smolhero",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL
    );

    gfxInit(g_hwnd, screenW, screenH);
    ShowWindow(g_hwnd, nCmdShow);

    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_pendingKey) {
            switch (state) {
                case STATE_WORLD:   handleWorldInput(g_pendingKey);   break;
                case STATE_COMBAT:  handleCombatInput(g_pendingKey);  break;
                case STATE_MENU:    handleMenuInput(g_pendingKey);    break;
                case STATE_DIALOG:  handleDialogInput(g_pendingKey);  break;
                case STATE_DUNGEON: handleDungeonInput(g_pendingKey); break;
                case STATE_TOWN:    handleTownInput(g_pendingKey);    break;
            }
            g_pendingKey = 0;
        }

        clearScreen();
        switch (state) {
            case STATE_WORLD:   renderWorld();   break;
            case STATE_COMBAT:  renderCombat();  break;
            case STATE_MENU:    renderMenu();    break;
            case STATE_DIALOG:  renderDialog();  break;
            case STATE_DUNGEON: renderDungeon(); break;
            case STATE_TOWN:    renderTown();    break;
        }

        gfxPresent(g_hwnd);
        Sleep(16);
    }

    gfxShutdown();
    return 0;
}
