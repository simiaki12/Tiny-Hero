#pragma once
#include <stdint.h>
#include "pak.h"
#include "game.h"

#define MAX_QUESTS    128
#define QUEST_MAX_OBJ   4

/* Objective types */
#define OBJ_KILL       0   /* kill required count of targetId enemy */
#define OBJ_GET_ITEM   1   /* acquire targetId item */
#define OBJ_VISIT_ZONE 2   /* step on map loc tile == targetId */
#define OBJ_TALK_NPC   3   /* complete dialog tree targetId */

/* Start trigger types */
#define TRIG_ALWAYS  0   /* active from game start */
#define TRIG_DIALOG  1   /* activated when dialog tree startId closes */
#define TRIG_ZONE    2   /* activated when player steps on loc tile == startId */
#define TRIG_ITEM    3   /* activated when player acquires item startId */
#define TRIG_KILL    4   /* activated when player kills enemy startId */

/* Quest flags */
#define QUEST_MAIN   (1<<0)
#define QUEST_HIDDEN (1<<1)

/* Quest status */
#define QUEST_INACTIVE 0
#define QUEST_ACTIVE   1
#define QUEST_DONE     2

typedef struct {
    uint8_t type;      /* OBJ_* */
    uint8_t targetId;  /* enemy index / item id / loc tile / dialog tree id */
    uint8_t required;  /* count required (1 for visit/talk) */
    uint8_t _pad;
} QuestObjective;  /* 4 bytes */

/* Binary format: [1 byte count][N × 48 bytes QuestDef] */
typedef struct {
    char           name[24];
    uint8_t        objectiveCount;
    uint8_t        rewardXp;
    uint8_t        rewardItemId;   /* 0xFF = no item reward */
    uint8_t        flags;          /* QUEST_* */
    QuestObjective objectives[4]; /* 4 × 4 = 16 bytes */
    uint8_t        startType;      /* TRIG_* */
    uint8_t        startId;        /* which dialog/zone/item/enemy triggers start */
    uint8_t        _pad[2];
} QuestDef;  /* 48 bytes */

typedef struct {
    uint8_t status[MAX_QUESTS];
    uint8_t progress[MAX_QUESTS][QUEST_MAX_OBJ];
} QuestState;

typedef struct {
    int       sel;
    GameState returnState;
} QuestLogState;

extern QuestDef      questDefs[MAX_QUESTS];
extern int           questDefCount;
extern QuestState    questSt;
extern QuestLogState questLogSt;

int  loadQuests(PakData data);

void questOnEnemyKilled(uint8_t enemyId);
void questOnItemGained(uint8_t itemId);
void questOnZoneEntered(uint8_t locTile);
void questOnDialogDone(int treeId);

void handleQuestLogInput(int key);
void renderQuestLog(void);
