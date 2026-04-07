#pragma once
#include <stdint.h>

/* Palette index 0 = transparent (skipped by drawSprite8).
   Tiles use only indices 1-15 (fully opaque).
   Player sprite uses 0 for see-through pixels. */
static const uint32_t TILE_PAL[] = {
    0x000000,  /*  0: transparent */
    0x1a4d00,  /*  1: dark green  */
    0x2d7a00,  /*  2: mid green   */
    0x5cb800,  /*  3: light green */
    0x454545,  /*  4: mortar/crack */
    0x757081,  /*  5: stone gray  */
    0xb8b8b8,  /*  6: stone hi    */
    0x3d1500,  /*  7: dark dirt   */
    0xf10c3b,  /*  8: blood red   */
    0xcc9900,  /*  9: gold roof   */
    0xe0d090,  /* 10: cream wall  */
    0x6b3a10,  /* 11: door brown  */
    0x75757b,  /* 12: cave shadow */
    0x3d0066,  /* 13: cave glow   */
    0x1428e0,  /* 14: hero blue   */
    0xf0c878,  /* 15: skin tan    */
};

/* --- Grass floor ------------------------------------------ */
static const uint8_t TILE_GRASS[64] = {
    2,2,2,3,2,2,2,2,
    2,3,2,2,2,2,1,2,
    2,2,2,2,3,2,2,2,
    2,2,1,2,2,2,2,2,
    2,2,2,2,2,3,2,2,
    1,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,3,
    2,2,2,3,2,2,2,2,
};

/* --- Stone wall (brick pattern) --------------------------- */
static const uint8_t TILE_WALL[64] = {
    5,5,5,4,5,5,5,5,
    5,5,5,4,5,5,5,5,
    4,4,4,4,4,4,4,4,
    5,5,5,5,5,4,5,5,
    5,5,5,5,5,4,5,5,
    4,4,4,4,4,4,4,4,
    5,4,5,5,5,5,5,5,
    5,4,5,5,5,5,5,5,
};

/* --- Enemy zones — one tile per pool (loc 0x01-0x0F wraps at 4) ----------- */

/* Pool 1: forest (Goblin, Wolf) — dark grass with claw marks */
static const uint8_t TILE_ENEMY_FOREST[64] = {
    2,1,2,2,2,1,2,2,
    1,2,2,2,2,2,1,2,
    2,2,8,2,2,8,2,2,
    2,2,4,2,2,4,2,2,
    2,2,4,2,2,4,2,2,
    2,2,8,2,2,8,2,2,
    1,2,2,2,2,2,1,2,
    2,1,2,2,2,1,2,2,
};

/* Pool 2: ruins (Skeleton, Bandit) — cracked stone with bone fragments */
static const uint8_t TILE_ENEMY_RUINS[64] = {
    5,5,5,4,5,5,5,5,
    5,6,5,4,5,5,5,5,
    4,4,4,4,4,4,4,4,
    5,5,5,5,5,4,6,5,
    5,5,6,5,5,4,5,5,
    4,4,4,4,4,4,4,4,
    5,5,4,5,5,5,5,5,
    5,5,4,5,6,5,5,5,
};

/* Pool 3: cave (Giant Spider) — dark floor with web strands */
static const uint8_t TILE_ENEMY_CAVE[64] = {
    12, 4,12,12,12,12, 4,12,
     4, 4, 4,12,12, 4, 4, 4,
    12, 4,12, 4, 4,12, 4,12,
    12,12, 4,12,12, 4,12,12,
    12,12, 4,12,12, 4,12,12,
    12, 4,12, 4, 4,12, 4,12,
     4, 4, 4,12,12, 4, 4, 4,
    12, 4,12,12,12,12, 4,12,
};

/* Pool 4: dark magic (Dark Mage) — near-black with glowing rune */
static const uint8_t TILE_ENEMY_DARK[64] = {
    12,12,12,13,12,12,12,12,
    12,12,13,12,13,12,12,12,
    12,13,12,12,12,13,12,12,
    13,12,12,13,13,12,12,13,
    13,12,12,13,13,12,12,13,
    12,13,12,12,12,13,12,12,
    12,12,13,12,13,12,12,12,
    12,12,12,13,12,12,12,12,
};

static const uint8_t *const TILE_ENEMY_POOLS[] = {
    TILE_ENEMY_FOREST,
    TILE_ENEMY_RUINS,
    TILE_ENEMY_CAVE,
    TILE_ENEMY_DARK,
};
#define TILE_ENEMY_POOL_COUNT 4

/* --- Town entrance (house front) -------------------------- */
static const uint8_t TILE_TOWN[64] = {
     9, 9, 9, 9, 9, 9, 9, 9,
     9, 9, 9, 9, 9, 9, 9, 9,
    10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,
    10,10,11,11,11,10,10,10,
    10,10,11,11,11,10,10,10,
    10,10,11,11,11,10,10,10,
    10,10,11,11,11,10,10,10,
};

/* --- Dungeon entrance (cave arch) ------------------------- */
static const uint8_t TILE_DUNGEON[64] = {
     5, 5, 5, 5, 5, 5, 5, 5,
     5, 5,12,12,12, 5, 5, 5,
     5,12,12,13,12,12, 5, 5,
     5,12,13,13,13,12, 5, 5,
     5,12,13,13,13,12, 5, 5,
     5, 5,12,13,12, 5, 5, 5,
     5, 5, 5,12, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5,
};

/* --- Portal (glowing ring) -------------------------------- */
static const uint8_t TILE_PORTAL[64] = {
    12,12,13,13,13,13,12,12,
    12,13,13,12,12,13,13,12,
    13,13,12,12,12,12,13,13,
    13,12,12,12,12,12,12,13,
    13,12,12,12,12,12,12,13,
    13,13,12,12,12,12,13,13,
    12,13,13,12,12,13,13,12,
    12,12,13,13,13,13,12,12,
};

/* --- Player (top-down, transparent bg) -------------------- */
static const uint8_t SPRITE_PLAYER[64] = {
     0, 12,0,0,0, 12, 0, 0,
     0, 12,12,12,12, 12, 0, 0,
     0,0,12,12,12,0, 0, 0,
     0,12,12,8,12,12, 0, 0,
     0,0,12,12,12,0, 0, 0,
     0,0,12,12,12,0, 0, 0,
     0, 0,5, 0,5, 0, 0, 0,
     0, 0,5, 0,5, 0, 0, 0,
};
