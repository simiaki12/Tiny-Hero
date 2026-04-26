#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "npcs.h"
#include "game.h"
#include "gfx.h"
#include "pak.h"
#include "town.h"
#include "world.h"
#include <stdio.h>
#include "quests.h"

NpcDef npcDefs[NPC_DEF_MAX];
int    npcDefCount = 0;

/* Lazily-loaded sprites per NPC slot (NULL = not yet loaded or no image) */
static PakData npcImgs[NPC_DEF_MAX];
static int     npcImgsLoaded = 0;

/* Binary format: [1 byte count][N × sizeof(NpcDef)] */
int loadNpcs(PakData data) {
    if (!data.data || data.size < 1) return 0;

    const uint8_t *d = (const uint8_t *)data.data;
    uint8_t count = d[0];
    if (count > NPC_DEF_MAX) count = NPC_DEF_MAX;

    uint32_t expected = 1 + (uint32_t)count * sizeof(NpcDef);
    if (data.size < expected) return 0;

    memcpy(npcDefs, d + 1, (size_t)count * sizeof(NpcDef));
    npcDefCount   = count;
    npcImgsLoaded = 0;
    return npcDefCount;
}

static void ensureImgsLoaded(void) {
    if (npcImgsLoaded) return;
    for (int i = 0; i < npcDefCount; i++) {
        if (npcDefs[i].imgName[0]) {
            char path[32];
            snprintf(path, sizeof(path), "assets/sprites/%.2s.bin", npcDefs[i].imgName);
            npcImgs[i] = pakRead(path);
        } else {
            npcImgs[i].data = NULL;
            npcImgs[i].size = 0;
        }
    }
    npcImgsLoaded = 1;
}

void renderNpcs(int tx, int ty, int rCamX, int rCamY) {
    if (npcDefCount == 0) return;
    ensureImgsLoaded();

    for (int i = 0; i < npcDefCount; i++) {
        const NpcDef *n = &npcDefs[i];
        if (!n->mapId[0]) continue;
        if (!strstr(currentMapName, n->mapId)) continue;
        if ((int)n->x != tx || (int)n->y != ty) continue;

        /* Iso screen position of this tile's top vertex */
        int cx = (tx - ty) * (TILE_W / 2) - rCamX + gfxWidth  / 2;
        int cy = (tx + ty) * (TILE_H / 2) - rCamY + gfxHeight / 2;

        if (npcImgs[i].data) {
            /* 8x8 sprite at scale 4 = 32x32, bottom-centred on tile face */
            drawBin(cx - 16, cy + TILE_H / 2 - 32, (const uint8_t *)npcImgs[i].data, 4, 0, 255);
        } else {
            fillRect(cx - 6, cy + TILE_H / 2 - 14, 12, 14, rgb(200, 0, 200));
        }
    }
}

void npcTryInteract(void) {
    if (npcDefCount == 0) return;

    for (int i = 0; i < npcDefCount; i++) {
        const NpcDef *n = &npcDefs[i];
        if (!n->mapId[0]) continue;
        if (!strstr(currentMapName, n->mapId)) continue;
        if (n->treeId == 0xFF) continue;

        int dx = (int)n->x - worldPlayerX;
        int dy = (int)n->y - worldPlayerY;
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1) continue;
        if (dx != 0 && dy != 0) continue; /* only cardinal adjacency */

        if (n->treeId < (uint8_t)dialogTreeCount) {
            questOnDialogDone(n->treeId);
            startDialog(n->treeId, STATE_WORLD);
            return;
        }
    }
}
