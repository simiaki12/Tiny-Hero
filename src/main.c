#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pak.h"
#include "player.h"
#include "gfx.h"

#define TILE_SIZE     32
#define MAX_MAP_TILES (256 * 256)

/* Map */
static int     mapWidth  = 0;
static int     mapHeight = 0;
static uint8_t mapGfx[MAX_MAP_TILES];
static uint8_t mapLoc[MAX_MAP_TILES];

/* Player position */
static int playerX = 2, playerY = 2;

/* State machine */
typedef enum { STATE_WORLD, STATE_COMBAT, STATE_MENU, STATE_DIALOG, STATE_DUNGEON } GameState;
static GameState state = STATE_WORLD;

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
        if (loc == LOC_ENEMY)   state = STATE_COMBAT;
        if (loc == LOC_TOWN)    state = STATE_DIALOG;
        if (loc == LOC_DUNGEON) state = STATE_DUNGEON;
    }
}

static void handleCombatInput(int key)  { if (key == VK_ESCAPE) state = STATE_WORLD; }
static void handleMenuInput(int key)    { if (key == VK_ESCAPE) state = STATE_WORLD; }
static void handleDialogInput(int key)  { if (key == VK_ESCAPE) state = STATE_WORLD; }
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

static void renderCombat(void) {
    /* Placeholder — combat system coming soon */
    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(80, 20, 20));
}

static void renderMenu(void) {
    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(20, 20, 80));

    int x = 60, y = 60;
    const int lineH = 20;
    char buf[32];

    drawText(x, y, "STATS", rgb(220, 220, 255), 2); y += lineH + 4;

    snprintf(buf, sizeof(buf), "HP:  %d", player.maxHp);
    drawText(x, y, buf, rgb(100, 220, 100), 2); y += lineH;

    snprintf(buf, sizeof(buf), "ATK: %d", player.attack);
    drawText(x, y, buf, rgb(220, 100, 100), 2); y += lineH;

    snprintf(buf, sizeof(buf), "DEF: %d", player.defense);
    drawText(x, y, buf, rgb(100, 160, 220), 2); y += lineH;

    snprintf(buf, sizeof(buf), "WPN: %d", player.weaponId);
    drawText(x, y, buf, rgb(200, 200, 100), 2); y += lineH;
    snprintf(buf, sizeof(buf), "ARM: %d", player.armorId);
    drawText(x, y, buf, rgb(200, 200, 100), 2); y += lineH + 4;

    if (player.abilityCount > 0) {
        drawText(x, y, "ABILITIES:", rgb(180, 120, 220), 2); y += lineH;
        for (int i = 0; i < player.abilityCount && i < 8; i++) {
            snprintf(buf, sizeof(buf), "  %d", player.abilities[i]);
            drawText(x, y, buf, rgb(180, 120, 220), 2); y += lineH;
        }
    }
}

static void renderDialog(void) {
    /* Placeholder — dialog system coming soon */
    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(150, 120, 0));
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
    pakClose();

    if (!loadMap(mapData) || !loadPlayer(playerData)) return 1;
    free(mapData.data);
    free(playerData.data);

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
                case STATE_MENU:    handleMenuInput(g_pendingKey);     break;
                case STATE_DIALOG:  handleDialogInput(g_pendingKey);  break;
                case STATE_DUNGEON: handleDungeonInput(g_pendingKey); break;
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
        }

        gfxPresent(g_hwnd);
        Sleep(16);
    }

    gfxShutdown();
    return 0;
}
