#pragma once
#include <stdint.h>
#include "enemies.h"

/* --- Enemy capability flags --- */
#define ENEMY_HAS_WEAPON  (1<<0)  /* can be disarmed */
#define ENEMY_EXECUTABLE  (1<<1)  /* can be finished with Execute when low HP */
#define ENEMY_BLOCKABLE   (1<<2)  /* defend reduces damage */
#define ENEMY_STUNNABLE   (1<<3)  /* can be stunned */

typedef struct {
    char    name[16];
    int     hp;
    int     maxHp;
    uint8_t attack;
    uint8_t defense;
    uint8_t size;         /* 1=tiny .. 5=huge; raises Strong weight */
    uint8_t speed;        /* raises Disarm/Backstab cost; lowers Strong weight */
    uint8_t intelligence; /* raises Calm weight */
    uint8_t perception;   /* lowers Hide/Backstab weight */
    uint8_t flags;        /* ENEMY_* bitfield */
    uint8_t xpReward;
} Enemy;

typedef enum { COMBAT_PHASE_ACTIVE, COMBAT_PHASE_VICTORY } CombatPhase;

/* --- Action system --- */

/* Context flags — conditions that must hold for an action to appear in the pool.
 * Add new flags here as enemy/player capabilities expand. */
#define ACT_CTX_FIRST_TURN   (1<<0)  /* only available on the opening turn */
#define ACT_CTX_ENEMY_WEAPON (1<<1)  /* only if enemy has a weapon */
#define ACT_CTX_EXECUTABLE   (1<<2)  /* only if enemy is executable AND HP < 1/3 max */
#define ACT_CTX_CAN_STUN     (1<<3)  /* only if enemy is stunnable */
#define ACT_CTX_PLAYER_HURT  (1<<4)  /* only if player HP < 50% */

typedef enum {
    ACTION_ATTACK,
    ACTION_STRONG,
    ACTION_HEAL,
    ACTION_DEFEND,
    ACTION_DISARM,
    ACTION_BACKSTAB,
    ACTION_STUN,
    ACTION_CALM,
    ACTION_HIDE,
    ACTION_EXECUTE,
    ACTION_COUNT
} ActionId;

/* Data-driven definition for each action.
 * Add new fields here without touching generateActions — only computeWeight
 * and checkContext need updating for new mechanics. */
typedef struct {
    ActionId id;
    uint8_t  requiredSkill; /* SKILL_* constant, or 0xFF = no skill required */
    uint8_t  requiredLevel; /* minimum skill value */
    uint8_t  contextFlags;  /* ACT_CTX_* — all bits must hold */
    uint8_t  baseWeight;    /* base selection weight before modifiers */
    uint8_t  power;         /* base power value passed to performPlayerAction */
} ActionDef;

typedef struct { ActionId type; uint8_t power; } Action;

typedef struct {
    Enemy       enemy;
    uint8_t     enemyDefId;      /* index into enemyDefs[], 0xFF if unknown */
    Action      actions[4];    /* at most 4 shown per turn */
    int         actionCount;
    int         selectedIndex;
    int         isFirstTurn;
    int         skipEnemyAttack;  /* set by actions like Backstab, Stun, Hide */
    CombatPhase phase;
    int         gainedXp;
    int         leveledUp;
} CombatState;

extern CombatState combat;

void startCombat(const EnemyDef *def);
void handleCombatInput(int key);
void renderCombat(void);
void returnToTown(void);
