/* tools/seed_quests.c — writes the initial assets/quests.dat
 * Run once: make seed_quests
 * After that, use the quest_editor to add and modify quests. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Mirror of quests.h constants */
#define QUEST_MAX_OBJ 4
#define OBJ_KILL       0
#define OBJ_GET_ITEM   1
#define OBJ_VISIT_ZONE 2
#define OBJ_TALK_NPC   3
#define TRIG_ALWAYS  0
#define TRIG_DIALOG  1
#define TRIG_ZONE    2
#define TRIG_ITEM    3
#define TRIG_KILL    4
#define QUEST_MAIN   (1<<0)
#define QUEST_HIDDEN (1<<1)

typedef struct {
    uint8_t type;
    uint8_t targetId;
    uint8_t required;
    uint8_t _pad;
} QuestObjective;

typedef struct {
    char           name[24];
    uint8_t        objectiveCount;
    uint8_t        rewardXp;
    uint8_t        rewardItemId;
    uint8_t        flags;
    QuestObjective objectives[4];
    uint8_t        startType;
    uint8_t        startId;
    uint8_t        _pad[2];
} QuestDef;  /* 48 bytes */

static QuestDef quests[] = {
    /* Quest 0: Goblin Slayer — active from start, kill 5 goblins */
    {
        "Goblin Slayer",
        1,          /* objectiveCount */
        30,         /* rewardXp */
        0xFF,       /* rewardItemId: none */
        QUEST_MAIN,
        {
            { OBJ_KILL, 0 /* Goblin = enemyDef 0 */, 5, 0 },
        },
        TRIG_ALWAYS, 0, {0}
    },
    /* Quest 1: Into the Wild — active from start, step on an enemy zone */
    {
        "Into the Wild",
        1,
        10,
        0xFF,
        0,
        {
            { OBJ_VISIT_ZONE, 0x01 /* pool 1 loc tile */, 1, 0 },
        },
        TRIG_ALWAYS, 0, {0}
    },
    /* Quest 2: Bandit Hunt — starts after talking to first NPC (tree 0) */
    {
        "Bandit Hunt",
        1,
        50,
        0xFF,
        QUEST_MAIN,
        {
            { OBJ_KILL, 3 /* Bandit = enemyDef 3 */, 3, 0 },
        },
        TRIG_DIALOG, 0, {0}
    },
};

int main(void) {
    FILE *f = fopen("assets/data/quests.dat", "wb");
    if (!f) { fprintf(stderr, "Cannot write assets/quests.dat\n"); return 1; }

    int n = (int)(sizeof(quests) / sizeof(quests[0]));
    fwrite(&(uint8_t){(uint8_t)n}, 1, 1, f);
    fwrite(quests, sizeof(QuestDef), (size_t)n, f);
    fclose(f);

    printf("Written assets/quests.dat  (%d quests, %d bytes)\n",
        n, 1 + n * (int)sizeof(QuestDef));
    return 0;
}
