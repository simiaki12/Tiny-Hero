#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "quests.h"
#include "game.h"
#include "gfx.h"
#include "player.h"
#include "items.h"
#include "enemies.h"

QuestDef      questDefs[MAX_QUESTS];
int           questDefCount = 0;
QuestState    questSt;
QuestLogState questLogSt;

/* Binary format: [1 byte count][N × sizeof(QuestDef)] */
int loadQuests(PakData data) {
    memset(&questSt, 0, sizeof(questSt));
    if (!data.data || data.size < 1) return 0;

    const uint8_t *d = (const uint8_t *)data.data;
    uint8_t count = d[0];
    if (count > MAX_QUESTS) count = MAX_QUESTS;

    uint32_t expected = 1 + (uint32_t)count * sizeof(QuestDef);
    if (data.size < expected) return 0;

    memcpy(questDefs, d + 1, (size_t)count * sizeof(QuestDef));
    questDefCount = count;

    /* Activate always-on quests immediately */
    for (int i = 0; i < questDefCount; i++) {
        if (questDefs[i].startType == TRIG_ALWAYS)
            questSt.status[i] = QUEST_ACTIVE;
    }
    return questDefCount;
}

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

/* Try to complete a quest: check all objectives, grant reward if done. */
static void checkCompletion(int qi) {
    const QuestDef *q = &questDefs[qi];
    for (int j = 0; j < q->objectiveCount; j++) {
        if (questSt.progress[qi][j] < q->objectives[j].required)
            return;
    }
    questSt.status[qi] = QUEST_DONE;
    if (q->rewardXp)              awardXp(q->rewardXp);
    if (q->rewardItemId != 0xFF)  addItem(q->rewardItemId);
}

/* Try to activate a quest based on a trigger type + id. */
static void tryActivate(uint8_t trigType, uint8_t trigId) {
    for (int i = 0; i < questDefCount; i++) {
        if (questSt.status[i] != QUEST_INACTIVE) continue;
        if (questDefs[i].startType == trigType && questDefs[i].startId == trigId)
            questSt.status[i] = QUEST_ACTIVE;
    }
}

/* Advance all active quests matching a given objective type + targetId. */
static void advanceObjectives(uint8_t objType, uint8_t targetId) {
    for (int i = 0; i < questDefCount; i++) {
        if (questSt.status[i] != QUEST_ACTIVE) continue;
        const QuestDef *q = &questDefs[i];
        for (int j = 0; j < q->objectiveCount; j++) {
            if (q->objectives[j].type != objType) continue;
            if (q->objectives[j].targetId != targetId) continue;
            if (questSt.progress[i][j] < q->objectives[j].required)
                questSt.progress[i][j]++;
        }
        checkCompletion(i);
    }
}

/* -----------------------------------------------------------------------
 * Public trigger hooks — called by game systems
 * ----------------------------------------------------------------------- */

void questOnEnemyKilled(uint8_t enemyId) {
    if (questDefCount == 0) return;
    tryActivate(TRIG_KILL, enemyId);
    advanceObjectives(OBJ_KILL, enemyId);
}

void questOnItemGained(uint8_t itemId) {
    if (questDefCount == 0) return;
    tryActivate(TRIG_ITEM, itemId);
    advanceObjectives(OBJ_GET_ITEM, itemId);
}

void questOnZoneEntered(uint8_t locTile) {
    if (questDefCount == 0) return;
    tryActivate(TRIG_ZONE, locTile);
    advanceObjectives(OBJ_VISIT_ZONE, locTile);
}

void questOnDialogDone(int treeId) {
    if (questDefCount == 0 || treeId < 0 || treeId > 254) return;
    tryActivate(TRIG_DIALOG, (uint8_t)treeId);
    advanceObjectives(OBJ_TALK_NPC, (uint8_t)treeId);
}

/* -----------------------------------------------------------------------
 * Quest log UI
 * ----------------------------------------------------------------------- */

static const char *objTypeName(uint8_t t) {
    switch (t) {
        case OBJ_KILL:       return "Kill";
        case OBJ_GET_ITEM:   return "Obtain";
        case OBJ_VISIT_ZONE: return "Visit";
        case OBJ_TALK_NPC:   return "Talk";
        default:             return "???";
    }
}

static void objTargetName(const QuestObjective *o, char *buf, int bufLen) {
    switch (o->type) {
        case OBJ_KILL:
            if (o->targetId < (uint8_t)enemyDefCount)
                snprintf(buf, (size_t)bufLen, "%s", enemyDefs[o->targetId].name);
            else
                snprintf(buf, (size_t)bufLen, "enemy #%d", o->targetId);
            break;
        case OBJ_GET_ITEM:
            snprintf(buf, (size_t)bufLen, "%s", itemName(o->targetId));
            break;
        case OBJ_VISIT_ZONE:
            snprintf(buf, (size_t)bufLen, "zone 0x%02X", o->targetId);
            break;
        case OBJ_TALK_NPC:
            snprintf(buf, (size_t)bufLen, "NPC tree %d", o->targetId);
            break;
        default:
            snprintf(buf, (size_t)bufLen, "???");
            break;
    }
}

void handleQuestLogInput(int key) {
    /* Count displayable quests (active + done, not inactive hidden) */
    int visible = 0;
    for (int i = 0; i < questDefCount; i++) {
        uint8_t st = questSt.status[i];
        if (st == QUEST_INACTIVE) continue;
        if ((questDefs[i].flags & QUEST_HIDDEN) && st == QUEST_INACTIVE) continue;
        visible++;
    }

    switch (key) {
        case VK_UP:
            if (questLogSt.sel > 0) questLogSt.sel--;
            break;
        case VK_DOWN:
            if (questLogSt.sel < visible - 1) questLogSt.sel++;
            break;
        case VK_ESCAPE:
        case 'J':
            state = questLogSt.returnState;
            break;
    }
}

void renderQuestLog(void) {
    const int boxX  = 40;
    const int boxY  = 40;
    const int boxW  = gfxWidth  - 80;
    const int boxH  = gfxHeight - 80;
    const int textX = boxX + 14;
    const int lineH = 20;
    const int scale = 2;

    fillRect(boxX, boxY, boxW, boxH, rgb(5, 15, 40));
    fillRect(boxX, boxY, boxW, 2, rgb(80, 120, 200));

    int y = boxY + 8;
    drawText(textX, y, "QUEST LOG", rgb(220, 200, 100), scale);
    drawText(textX + 200, y, "J / Esc to close", rgb(80, 80, 100), 1);
    y += lineH + 4;

    if (questDefCount == 0) {
        drawText(textX, y, "No quests loaded.", rgb(100, 100, 100), scale);
        return;
    }

    /* Separate active and done quests into two sections */
    int visIdx  = 0;
    int anyShown = 0;

    /* Section: Active */
    int activeCount = 0;
    for (int i = 0; i < questDefCount; i++)
        if (questSt.status[i] == QUEST_ACTIVE) activeCount++;

    if (activeCount > 0) {
        drawText(textX, y, "-- Active --", rgb(100, 140, 200), 1);
        y += 14;

        for (int i = 0; i < questDefCount; i++) {
            if (questSt.status[i] != QUEST_ACTIVE) continue;
            if (y + lineH > boxY + boxH - 10) break;

            const QuestDef *q = &questDefs[i];
            int sel = (visIdx == questLogSt.sel);
            uint32_t nameColor = sel ? rgb(255, 255, 100) : rgb(220, 220, 220);

            char buf[64];
            snprintf(buf, sizeof(buf), "%s%s%s",
                sel ? "> " : "  ",
                q->name,
                (q->flags & QUEST_MAIN) ? "  [main]" : "");
            drawText(textX, y, buf, nameColor, scale);
            y += lineH;

            /* Show objectives for selected quest */
            if (sel) {
                for (int j = 0; j < q->objectiveCount; j++) {
                    if (y + 14 > boxY + boxH - 10) break;
                    const QuestObjective *o = &q->objectives[j];
                    char tgtBuf[20];
                    objTargetName(o, tgtBuf, sizeof(tgtBuf));
                    uint8_t prog = questSt.progress[i][j];
                    int done = (prog >= o->required);
                    snprintf(buf, sizeof(buf), "    %s %s: %d/%d%s",
                        objTypeName(o->type), tgtBuf,
                        prog, o->required,
                        done ? " [done]" : "");
                    drawText(textX, y, buf, done ? rgb(100, 200, 100) : rgb(160, 160, 160), 1);
                    y += 14;
                }
                y += 4;
            }

            visIdx++;
            anyShown = 1;
        }
    }

    /* Section: Completed */
    int doneCount = 0;
    for (int i = 0; i < questDefCount; i++)
        if (questSt.status[i] == QUEST_DONE) doneCount++;

    if (doneCount > 0 && y + 14 < boxY + boxH - 10) {
        y += 4;
        drawText(textX, y, "-- Completed --", rgb(80, 120, 80), 1);
        y += 14;

        for (int i = 0; i < questDefCount; i++) {
            if (questSt.status[i] != QUEST_DONE) continue;
            if (y + lineH > boxY + boxH - 10) break;

            int sel = (visIdx == questLogSt.sel);
            char buf[48];
            snprintf(buf, sizeof(buf), "%s[x] %s",
                sel ? "> " : "  ",
                questDefs[i].name);
            drawText(textX, y, buf, sel ? rgb(180, 220, 180) : rgb(80, 130, 80), scale);
            y += lineH;
            visIdx++;
            anyShown = 1;
        }
    }

    if (!anyShown)
        drawText(textX, y, "No active quests.", rgb(100, 100, 100), scale);
}
