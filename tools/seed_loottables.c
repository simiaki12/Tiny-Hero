/* tools/seed_loottables.c — writes assets/loottables.dat
 * Run once: make seed_loottables
 * Edit tables here and re-run to regenerate.
 *
 * chance: 0-255  (128 = ~50%,  255 = always,  0 = never)
 * questId: 0xFF  = no quest requirement
 * questStatus: QUEST_INACTIVE=0  QUEST_ACTIVE=1  QUEST_DONE=2
 *
 * Item IDs (keep in sync with seed_items.c):
 *   0  Iron Sword
 *   1  Leather Armor
 *   2  Health Potion
 *   3  Flaming Sword
 */

#include <stdio.h>
#include <stdint.h>

#define QUEST_ACTIVE 1
#define QUEST_DONE   2
#define NO_QUEST     0xFF

typedef struct {
    uint8_t itemId;
    uint8_t chance;
    uint8_t questId;
    uint8_t questStatus;
} LootEntry;

typedef struct {
    LootEntry entries[8];
    int       count;
} Table;

/* -------------------------------------------------------------------
 * Table definitions
 * Table index must match lootTableId in seed_enemies.c
 * ------------------------------------------------------------------- */
static Table tables[] = {
    /* Table 0: Goblin drops */
    { .entries = {
        { 3, 128, NO_QUEST, 0 },   /* Flaming Sword — 50% chance, always available */
    }, .count = 1 },
};

int main(void) {
    FILE *f = fopen("assets/loottables.dat", "wb");
    if (!f) { fprintf(stderr, "Cannot write assets/loottables.dat\n"); return 1; }

    int nTables = (int)(sizeof(tables) / sizeof(tables[0]));
    fwrite(&(uint8_t){(uint8_t)nTables}, 1, 1, f);

    int totalBytes = 1;
    for (int t = 0; t < nTables; t++) {
        uint8_t ec = (uint8_t)tables[t].count;
        fwrite(&ec, 1, 1, f);
        fwrite(tables[t].entries, sizeof(LootEntry), ec, f);
        totalBytes += 1 + ec * (int)sizeof(LootEntry);
    }

    fclose(f);
    printf("Written assets/loottables.dat  (%d tables, %d bytes)\n", nTables, totalBytes);
    return 0;
}
