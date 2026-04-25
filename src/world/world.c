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
#include "npcs.h"
#include "tiles8x8.h"
#include "pak.h"

/* ── iso tile images, lazy-loaded on first render ── */
#define TIMG_GRASS       0
#define TIMG_GRASS_ENEMY 1
#define TIMG_GRASS_TOWN  2
#define TIMG_GRASS_DUNG  3
#define TIMG_GRASS_PORT  4
#define TIMG_RIVER       5
#define TIMG_ROAD        6
#define TIMG_BRIDGE      7
#define TIMG_BLDG_FLOOR  8
#define TIMG_CAVE_FLOOR  9
#define TIMG_HILLS       10
#define TIMG_WALL        11
#define TIMG_CAVE_WALL   12
#define TIMG_MOUNTAINS   13
#define TIMG_TREE        14
#define N_TILE_IMGS      15

static PakData g_tileImgs[N_TILE_IMGS];
static int     g_tilesLoaded = 0;

static void loadTileImgs(void) {
    static const char *paths[N_TILE_IMGS] = {
        "assets/tiles/grass.til",
        "assets/tiles/grass_enemy.bin",
        "assets/tiles/grass_town.bin",
        "assets/tiles/grass_dungeon.bin",
        "assets/tiles/grass_portal.bin",
        "assets/tiles/river.bin",
        "assets/tiles/road.bin",
        "assets/tiles/bridge.bin",
        "assets/tiles/bldg_floor.bin",
        "assets/tiles/cave_floor.bin",
        "assets/tiles/hills.bin",
        "assets/tiles/wall.bin",
        "assets/tiles/cave_wall.bin",
        "assets/tiles/mountains.bin",
        "assets/tiles/tree.bin",
    };
    for (int i = 0; i < N_TILE_IMGS; i++)
        g_tileImgs[i] = pakRead(paths[i]);
    g_tilesLoaded = 1;
}

int     worldPlayerX  = 2;
int     worldPlayerY  = 2;
static int   g_walkFrame      = 0;
static int   g_moving         = 0;
/* Camera slide: oldCamX - newCamX. May be 0 or partial near edges. */
static int   g_camSlideStartX = 0;
static int   g_camSlideStartY = 0;
static int   g_camSlideX      = 0;
static int   g_camSlideY      = 0;
/* Player offset: always ±TILE_SIZE, independent of camera movement. */
static int   g_plrOffStartX   = 0;
static int   g_plrOffStartY   = 0;
static int   g_plrOffX        = 0;
static int   g_plrOffY        = 0;
static DWORD g_moveStart      = 0;
static int   g_midToggled     = 0;
#define SLIDE_MS 200
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
    /* Camera tracks player in isometric pixel space; no clamping needed. */
    camX = (worldPlayerX - worldPlayerY) * (TILE_W / 2);
    camY = (worldPlayerX + worldPlayerY) * (TILE_H / 2);
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

void updateWorld(void) {
    if (!g_moving) return;
    DWORD elapsed = GetTickCount() - g_moveStart;
    if (elapsed >= SLIDE_MS) {
        g_camSlideX = 0;
        g_camSlideY = 0;
        g_plrOffX   = 0;
        g_plrOffY   = 0;
        g_moving    = 0;
    } else {
        if (!g_midToggled && elapsed >= SLIDE_MS / 2) {
            g_walkFrame  ^= 1;
            g_midToggled  = 1;
        }
        int rem      = (int)(SLIDE_MS - elapsed);
        g_camSlideX  = g_camSlideStartX * rem / SLIDE_MS;
        g_camSlideY  = g_camSlideStartY * rem / SLIDE_MS;
        g_plrOffX    = g_plrOffStartX   * rem / SLIDE_MS;
        g_plrOffY    = g_plrOffStartY   * rem / SLIDE_MS;
    }
}

void handleWorldInput(int key) {
    if (key == 'I') { state = STATE_INVENTORY; return; }
    if (key == 'E') { npcTryInteract(); return; }
    if (g_moving) return;

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
        IS_GFX_PASSABLE(mapGfx[newY * mapWidth + newX])) {
        int dx       = newX - worldPlayerX;
        int dy       = newY - worldPlayerY;
        int oldCamX  = camX;
        int oldCamY  = camY;
        worldPlayerX = newX;
        worldPlayerY = newY;
        g_walkFrame ^= 1;
        worldUpdateCamera();
        /* Camera slide: whatever the camera actually moved (0 when clamped). */
        g_camSlideStartX = oldCamX - camX;
        g_camSlideStartY = oldCamY - camY;
        g_camSlideX      = g_camSlideStartX;
        g_camSlideY      = g_camSlideStartY;
        /* Player offset in iso pixel space: starts at old iso position. */
        g_plrOffStartX   = -(dx - dy) * (TILE_W / 2);
        g_plrOffStartY   = -(dx + dy) * (TILE_H / 2);
        g_plrOffX        = g_plrOffStartX;
        g_plrOffY        = g_plrOffStartY;
        g_moveStart   = GetTickCount();
        g_midToggled  = 0;
        g_moving      = 1;

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

void returnToTown(void) {
    worldPlayerX = 2;
    worldPlayerY = 2;
    worldUpdateCamera();    
}

void renderWorld(void) {
    if (!g_tilesLoaded) loadTileImgs();

    int rCamX = camX + g_camSlideX;
    int rCamY = camY + g_camSlideY;

    fillRect(0, 0, gfxWidth, gfxHeight, rgb(10, 10, 15));

    /* Painter's algorithm: draw back-to-front by ascending (tx+ty) sum */
    for (int sum = 0; sum < mapWidth + mapHeight - 1; sum++) {
        for (int tx = 0; tx <= sum; tx++) {
            int ty = sum - tx;
            if (tx < 0 || tx >= mapWidth || ty < 0 || ty >= mapHeight) continue;

            int cx = (tx - ty) * (TILE_W / 2) - rCamX + gfxWidth  / 2;
            int cy = (tx + ty) * (TILE_H / 2) - rCamY + gfxHeight / 2;

            /* Cull tiles fully off-screen (tallest tile is 64px) */
            if (cx + TILE_W / 2 < 0 || cx - TILE_W / 2 > gfxWidth)  continue;
            if (cy + 96 < 0 || cy > gfxHeight + 2 * TILE_H)            continue;

            uint8_t gfx = mapGfx[ty * mapWidth + tx];
            uint8_t loc = mapLoc[ty * mapWidth + tx];
            int img_idx;

            switch (gfx) {
                case GFX_WALL:           img_idx = TIMG_WALL;       break;
                case GFX_TREE:           img_idx = TIMG_TREE;       break;
                case GFX_RIVER:          img_idx = TIMG_RIVER;      break;
                case GFX_BRIDGE:         img_idx = TIMG_BRIDGE;     break;
                case GFX_ROAD:           img_idx = TIMG_ROAD;       break;
                case GFX_BUILDING_FLOOR: img_idx = TIMG_BLDG_FLOOR; break;
                case GFX_HILLS:          img_idx = TIMG_HILLS;       break;
                case GFX_MOUNTAINS:      img_idx = TIMG_MOUNTAINS;  break;
                case GFX_CAVE_FLOOR:     img_idx = TIMG_CAVE_FLOOR; break;
                case GFX_CAVE_WALL:      img_idx = TIMG_CAVE_WALL;  break;
                default:
                    if      (IS_ENEMY_POOL(loc)) img_idx = TIMG_GRASS_ENEMY;
                    else if (loc == LOC_TOWN)    img_idx = TIMG_GRASS_TOWN;
                    else if (loc == LOC_DUNGEON) img_idx = TIMG_GRASS_DUNG;
                    else if (IS_PORTAL(loc))     img_idx = TIMG_GRASS_PORT;
                    else                         img_idx = TIMG_GRASS;
                    break;
            }

            PakData *td = &g_tileImgs[img_idx];
            if (td->data) {
                /* Shift tall tiles up so their wall base sits at ground level */
                int draw_y = cy + TILE_H - (int)((const uint8_t *)td->data)[1] * 2;
                drawBin(cx - TILE_W / 2, draw_y, (const uint8_t *)td->data, 2);
            }

            /* Draw NPCs and player inline at correct painter depth */
            renderNpcs(tx, ty, rCamX, rCamY);
            if (tx == worldPlayerX && ty == worldPlayerY) {
                int px = cx + g_plrOffX;
                int py = cy + g_plrOffY;
                const uint8_t *spr = g_walkFrame ? SPRITE_PLAYER_2 : SPRITE_PLAYER;
                drawSprite8(px - 16, py + TILE_H / 2 - 32, spr, TILE_PAL, 4);
            }
        }
    }
}
