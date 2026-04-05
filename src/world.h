#pragma once
#include <stdint.h>
#include "pak.h"

#define TILE_SIZE     64
#define MAX_MAP_TILES (256 * 256)

typedef enum { LOC_EMPTY = 0, LOC_ENEMY = 1, LOC_TOWN = 2, LOC_DUNGEON = 3 } LocType;

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
