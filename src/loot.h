#pragma once
#include <stdint.h>
#include "pak.h"

#define LOOT_TABLE_MAX  32
#define LOOT_ENTRY_MAX   8
#define LOOT_DROP_MAX    4   /* max items collected in one fight */

/* 4 bytes — one conditional drop entry */
typedef struct {
    uint8_t itemId;
    uint8_t chance;      /* drop if rand()%256 < chance  (128 = ~50%, 255 = always) */
    uint8_t questId;     /* 0xFF = no quest requirement */
    uint8_t questStatus; /* required questSt.status[questId] value (QUEST_ACTIVE etc.) */
} LootEntry;

typedef struct {
    LootEntry entries[LOOT_ENTRY_MAX];
    uint8_t   count;
} LootTable;

/* File format (loottables.dat):
 *   [1]  table count
 *   for each table:
 *     [1]      entry count
 *     [N × 4]  LootEntry array
 */

extern LootTable lootTables[LOOT_TABLE_MAX];
extern int       lootTableCount;

int  loadLootTables(PakData data);

/* Roll all entries in tableId; appends dropped item IDs to dropsOut[].
 * dropCountOut must be initialised by the caller; capped at LOOT_DROP_MAX. */
void rollLoot(uint8_t tableId, uint8_t *dropsOut, int *dropCountOut);
