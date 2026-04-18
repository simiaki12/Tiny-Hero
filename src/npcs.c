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
            snprintf(path, sizeof(path), "assets/%.2s.bin", npcDefs[i].imgName);
            npcImgs[i] = pakRead(path);
        } else {
            npcImgs[i].data = NULL;
            npcImgs[i].size = 0;
        }
    }
    npcImgsLoaded = 1;
}

void renderNpcs(int rCamX, int rCamY) {
    if (npcDefCount == 0) return;
    ensureImgsLoaded();

    for (int i = 0; i < npcDefCount; i++) {
        const NpcDef *n = &npcDefs[i];
        if (!n->mapId[0]) continue;
        if (!strstr(currentMapName, n->mapId)) continue;

        int sx = n->x * TILE_SIZE - rCamX;
        int sy = n->y * TILE_SIZE - rCamY;

        /* Skip NPCs outside the visible area */
        if (sx + TILE_SIZE < 0 || sx > gfxWidth)  continue;
        if (sy + TILE_SIZE < 0 || sy > gfxHeight) continue;

        if (npcImgs[i].data) {
            int scale = TILE_SIZE / 8;
            drawBin(sx, sy, (const uint8_t *)npcImgs[i].data, scale);
        } else {
            /* Fallback: magenta square */
            fillRect(sx + 8, sy + 8, TILE_SIZE - 16, TILE_SIZE - 16, rgb(200, 0, 200));
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
