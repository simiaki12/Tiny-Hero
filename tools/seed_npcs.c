/* tools/seed_npcs.c — writes the initial assets/npcs.dat
 * Run once: make seed_npcs
 * After that, use the npc_editor or edit here and re-run. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    char    name[16];
    char    imgName[3];
    uint8_t treeId;
    uint8_t x;
    uint8_t y;
    char    mapId[8];
    uint8_t _pad[2];
} NpcDef;

typedef char check_size[(sizeof(NpcDef) == 32) ? 1 : -1];

static NpcDef defs[] = {
    /* name           img  tree  x   y  mapId      pad */
    { "Village Elder", "op", 0,  1,  1, "map1",   {0} },
    { "Merchant",      "sh", 1,  3,  3, "map1",   {0} },
};

int main(void) {
    FILE *f = fopen("assets/data/npcs.dat", "wb");
    if (!f) { fprintf(stderr, "Cannot write assets/npcs.dat\n"); return 1; }

    int n = (int)(sizeof(defs) / sizeof(defs[0]));
    fwrite(&(uint8_t){(uint8_t)n}, 1, 1, f);
    fwrite(defs, sizeof(NpcDef), n, f);
    fclose(f);

    printf("Written assets/npcs.dat  (%d NPCs, %d bytes)\n",
        n, 1 + n * (int)sizeof(NpcDef));
    return 0;
}
