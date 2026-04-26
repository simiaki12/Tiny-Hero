#pragma once
#include <stdint.h>
#include "pak.h"

#define TILE_SIZE     64
#define TILE_W        64          /* iso diamond screen width  */
#define TILE_H        32          /* iso diamond screen height */
#define WALL_H        TILE_H      /* wall front-face height    */
#define MAX_MAP_TILES (256 * 256)

/* mapGfx tile type constants */
#define GFX_GRASS          0
#define GFX_WALL           1
#define GFX_TREE           2
#define GFX_RIVER          3
#define GFX_BRIDGE         4
#define GFX_ROAD           5
#define GFX_BUILDING_FLOOR 6
#define GFX_HILLS          7
#define GFX_MOUNTAINS      8
#define GFX_CAVE_FLOOR     9
#define GFX_CAVE_WALL      10
#define GFX_TAVERN_WALL    11

#define IS_GFX_PASSABLE(g) ((g)==GFX_GRASS||(g)==GFX_BRIDGE||(g)==GFX_ROAD||(g)==GFX_BUILDING_FLOOR||(g)==GFX_HILLS||(g)==GFX_CAVE_FLOOR)

/* Loc tile values stored in mapLoc[]:
 *   0x00        = empty (walkable, no event)
 *   0x01-0x0F   = enemy pool IDs 1-15 (step triggers combat from that pool)
 *   0xFE        = town entrance
 *   0xFF        = dungeon entrance / map transition
 */
#define LOC_EMPTY    0x00
#define LOC_TOWN     0xFE
#define LOC_DUNGEON  0xFF
#define IS_ENEMY_POOL(l) ((uint8_t)(l) >= 0x01 && (uint8_t)(l) <= 0x0F)

#define LOC_PORTAL_BASE  0xE0
#define MAX_PORTALS      16
#define IS_PORTAL(l)     ((uint8_t)(l) >= LOC_PORTAL_BASE && (uint8_t)(l) < LOC_PORTAL_BASE + MAX_PORTALS)
#define PORTAL_ID(l)     ((uint8_t)(l) - LOC_PORTAL_BASE)

extern int     worldPlayerX;
extern int     worldPlayerY;
extern int     camX;
extern int     camY;
extern int     mapWidth;
extern int     mapHeight;
extern uint8_t mapGfx[MAX_MAP_TILES];
extern uint8_t mapLoc[MAX_MAP_TILES];

/* Name of the map this map's LOC_DUNGEON tiles lead to. Empty = no transition. */
extern char mapTransitionTarget[64];
extern char currentMapName[64];
extern char    portalTargets[MAX_PORTALS][64];
extern uint8_t portalSpawnX[MAX_PORTALS];
extern uint8_t portalSpawnY[MAX_PORTALS];

/* Loads a map by pak name. Sets worldPlayerX/Y to the map's embedded spawn point.
   The pak must already be open. Returns 1 on success, 0 on failure. */
int  worldLoadNamed(const char *name);
void worldUpdateCamera(void);

void updateWorld(void);
void handleWorldInput(int key);
void renderWorld(void);
