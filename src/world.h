#pragma once
#include <stdint.h>
#include "pak.h"

#define TILE_SIZE     64
#define MAX_MAP_TILES (256 * 256)

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

/* Loads a map by pak name. Sets worldPlayerX/Y to the map's embedded spawn point.
   The pak must already be open. Returns 1 on success, 0 on failure. */
int  worldLoadNamed(const char *name);
void worldUpdateCamera(void);

void handleWorldInput(int key);
void renderWorld(void);
