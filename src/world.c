#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "world.h"
#include "game.h"
#include "gfx.h"
#include "enemies.h"
#include "combat.h"
#include "town.h"
#include "player.h"
#include "quests.h"
#include "tiles8x8.h"

int     worldPlayerX = 2;
int     worldPlayerY = 2;
int     camX         = 0;
int     camY         = 0;
int     mapWidth     = 0;
int     mapHeight    = 0;
uint8_t mapGfx[MAX_MAP_TILES];
uint8_t mapLoc[MAX_MAP_TILES];
char    mapTransitionTarget[64]        = {0};
char    currentMapName[64]             = {0};
char    portalTargets[MAX_PORTALS][64] = {{0}};
uint8_t portalSpawnX[MAX_PORTALS]     = {0};
uint8_t portalSpawnY[MAX_PORTALS]     = {0};

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
 *     [1 byte]:  portal count P
 *     [P times]: { 1-byte portal_id, 1-byte spawnX, 1-byte spawnY, null-terminated dest pak name }
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
    memset(portalTargets, 0, sizeof(portalTargets));
    int spawnX = 2, spawnY = 2;

    int pos = 2 + n * 2;
    if ((int)data.size > pos + 1) {
        spawnX = data.data[pos];
        spawnY = data.data[pos + 1];
        pos   += 2;

        /* null-terminated transition target */
        int len = 0;
        while (pos + len < (int)data.size && data.data[pos + len] != '\0' && len < 63) len++;
        memcpy(mapTransitionTarget, data.data + pos, len);
        mapTransitionTarget[len] = '\0';
        pos += len + 1;

        /* portal table: [count][id, spawnX, spawnY, dest\0] ... */
        memset(portalSpawnX, 0, sizeof(portalSpawnX));
        memset(portalSpawnY, 0, sizeof(portalSpawnY));
        if (pos < (int)data.size) {
            int pc = data.data[pos++];
            for (int i = 0; i < pc && pos + 2 < (int)data.size; i++) {
                int pid = data.data[pos++];
                int sx  = data.data[pos++];
                int sy  = data.data[pos++];
                len = 0;
                while (pos + len < (int)data.size && data.data[pos + len] != '\0' && len < 63) len++;
                if (pid < MAX_PORTALS) {
                    memcpy(portalTargets[pid], data.data + pos, len);
                    portalTargets[pid][len] = '\0';
                    portalSpawnX[pid] = (uint8_t)sx;
                    portalSpawnY[pid] = (uint8_t)sy;
                }
                pos += len + 1;
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
    if (ok) {
        strncpy(currentMapName, name, sizeof(currentMapName) - 1);
        currentMapName[sizeof(currentMapName) - 1] = '\0';
        worldUpdateCamera();
    }
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

        uint8_t loc = mapLoc[newY * mapWidth + newX];
        if (loc) questOnZoneEntered(loc);
        if (IS_ENEMY_POOL(loc)) { startCombatFromPool(loc); return; }
        if (loc == LOC_TOWN)    { startTown(); return; }
        if (loc == LOC_DUNGEON) {
            if (mapTransitionTarget[0] != '\0')
                worldLoadNamed(mapTransitionTarget);
            else
                state = STATE_DUNGEON;
            return;
        }
        if (IS_PORTAL(loc)) {
            int pid = PORTAL_ID(loc);
            if (portalTargets[pid][0] != '\0') {
                char dest[64];
                int sx = portalSpawnX[pid];
                int sy = portalSpawnY[pid];
                strncpy(dest, portalTargets[pid], sizeof(dest) - 1);
                dest[sizeof(dest) - 1] = '\0';
                worldLoadNamed(dest);
                worldPlayerX = sx;
                worldPlayerY = sy;
                worldUpdateCamera();
            }
            return;
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

    int scale = TILE_SIZE / 8;
    for (int y = tileY0; y < tileY1; y++) {
        for (int x = tileX0; x < tileX1; x++) {
            uint8_t  loc  = mapLoc[y * mapWidth + x];
            const uint8_t *tile;
            if (mapGfx[y * mapWidth + x] == 1) {
                tile = TILE_WALL;
            } else if (IS_ENEMY_POOL(loc)) {
                tile = TILE_ENEMY_POOLS[(loc - 1) % TILE_ENEMY_POOL_COUNT];
            } else if (loc == LOC_TOWN) {
                tile = TILE_TOWN;
            } else if (loc == LOC_DUNGEON) {
                tile = TILE_DUNGEON;
            } else if (IS_PORTAL(loc)) {
                tile = TILE_PORTAL;
            } else {
                tile = TILE_GRASS;
            }
            drawSprite8(x * TILE_SIZE - camX, y * TILE_SIZE - camY, tile, TILE_PAL, scale);
        }
    }
    drawSprite8(worldPlayerX * TILE_SIZE - camX, worldPlayerY * TILE_SIZE - camY,
                SPRITE_PLAYER, TILE_PAL, scale);
}
