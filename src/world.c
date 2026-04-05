#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "world.h"
#include "game.h"
#include "gfx.h"
#include "combat.h"
#include "town.h"

int     worldPlayerX = 2;
int     worldPlayerY = 2;
int     camX         = 0;
int     camY         = 0;
int     mapWidth     = 0;
int     mapHeight    = 0;
uint8_t mapGfx[MAX_MAP_TILES];
uint8_t mapLoc[MAX_MAP_TILES];
char    mapTransitionTarget[64] = {0};

void worldUpdateCamera(void) {
    camX = worldPlayerX * TILE_SIZE + TILE_SIZE / 2 - gfxWidth  / 2;
    camY = worldPlayerY * TILE_SIZE + TILE_SIZE / 2 - gfxHeight / 2;

    int maxCamX = mapWidth  * TILE_SIZE - gfxWidth;
    int maxCamY = mapHeight * TILE_SIZE - gfxHeight;

    /* if map is smaller than the viewport, center it */
    if (maxCamX <= 0) { camX = maxCamX / 2; }
    else              { if (camX < 0) camX = 0; if (camX > maxCamX) camX = maxCamX; }

    if (maxCamY <= 0) { camY = maxCamY / 2; }
    else              { if (camY < 0) camY = 0; if (camY > maxCamY) camY = maxCamY; }
}

/* Map binary layout:
 *   byte 0:      mapWidth
 *   byte 1:      mapHeight
 *   [w*h bytes]: mapGfx
 *   [w*h bytes]: mapLoc
 *   [optional extension, if bytes remain:]
 *     byte:      spawnX
 *     byte:      spawnY
 *     [null-terminated string]: transition target pak name (empty = none)
 */
static int loadMap(PakData data) {
    if (data.size < 2) return 0;
    mapWidth  = data.data[0];
    mapHeight = data.data[1];
    int n = mapWidth * mapHeight;
    if ((int)data.size < 2 + n * 2) return 0;
    memcpy(mapGfx, data.data + 2,     n);
    memcpy(mapLoc, data.data + 2 + n, n);

    mapTransitionTarget[0] = '\0';
    int spawnX = 2, spawnY = 2;

    int extOffset = 2 + n * 2;
    if ((int)data.size > extOffset + 1) {
        spawnX = data.data[extOffset];
        spawnY = data.data[extOffset + 1];
        const char *src = (const char *)(data.data + extOffset + 2);
        int remaining   = (int)data.size - extOffset - 2;
        if (remaining > 0) {
            int len = 0;
            while (len < remaining && src[len] != '\0') len++;
            if (len < 63) {
                memcpy(mapTransitionTarget, src, len);
                mapTransitionTarget[len] = '\0';
            }
        }
    }

    worldPlayerX = spawnX;
    worldPlayerY = spawnY;
    return 1;
}

int worldLoadNamed(const char *name) {
    PakData data = pakRead(name);
    if (!data.data) return 0;
    int ok = loadMap(data);
    free(data.data);
    if (ok) worldUpdateCamera();
    return ok;
}

void handleWorldInput(int key) {
    if (key == 'I') { state = STATE_MENU; return; }

    int newX = worldPlayerX, newY = worldPlayerY;
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
        worldPlayerX = newX;
        worldPlayerY = newY;
        worldUpdateCamera();

        LocType loc = (LocType)mapLoc[newY * mapWidth + newX];
        if (loc == LOC_ENEMY) { startCombat(); return; }
        if (loc == LOC_TOWN)  { startTown();   return; }
        if (loc == LOC_DUNGEON) {
            if (mapTransitionTarget[0] != '\0')
                worldLoadNamed(mapTransitionTarget);
            else
                state = STATE_DUNGEON;
        }
    }
}

void renderWorld(void) {
    /* Cull to only tiles that overlap the viewport */
    int tileX0 = camX / TILE_SIZE;
    int tileY0 = camY / TILE_SIZE;
    int tileX1 = (camX + gfxWidth  + TILE_SIZE - 1) / TILE_SIZE;
    int tileY1 = (camY + gfxHeight + TILE_SIZE - 1) / TILE_SIZE;
    if (tileX0 < 0) tileX0 = 0;
    if (tileY0 < 0) tileY0 = 0;
    if (tileX1 > mapWidth)  tileX1 = mapWidth;
    if (tileY1 > mapHeight) tileY1 = mapHeight;

    for (int y = tileY0; y < tileY1; y++) {
        for (int x = tileX0; x < tileX1; x++) {
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
            fillRect(x * TILE_SIZE - camX, y * TILE_SIZE - camY, TILE_SIZE, TILE_SIZE, color);
        }
    }
    fillRect(worldPlayerX * TILE_SIZE - camX, worldPlayerY * TILE_SIZE - camY,
             TILE_SIZE, TILE_SIZE, rgb(0, 20, 250));
}
