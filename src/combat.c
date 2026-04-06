#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "combat.h"
#include "game.h"
#include "player.h"
#include "items.h"
#include "skills.h"
#include "gfx.h"

CombatState combat;

/* --- Action table ---
 * To add a new action: add an ActionId to the enum in combat.h, add a row here,
 * add a case in computeWeight if it needs stat modifiers, and add a case in
 * performPlayerAction. Nothing else needs to change. */
static const ActionDef actionDefs[ACTION_COUNT] = {
    /*               id             reqSkill        reqLvl  ctxFlags             baseWt  power */
    /* ACTION_ATTACK   */ { ACTION_ATTACK,   0xFF,           0,      0,                   70,     0  },
    /* ACTION_STRONG   */ { ACTION_STRONG,   SKILL_BLADES,   2,      0,                   35,     4  },
    /* ACTION_HEAL     */ { ACTION_HEAL,     SKILL_SURVIVAL, 1,      ACT_CTX_PLAYER_HURT, 55,     10 },
    /* ACTION_DEFEND   */ { ACTION_DEFEND,   0xFF,           0,      0,                   28,     0  },
    /* ACTION_DISARM   */ { ACTION_DISARM,   SKILL_BLADES,   3,      ACT_CTX_ENEMY_WEAPON,48,     0  },
    /* ACTION_BACKSTAB */ { ACTION_BACKSTAB, SKILL_SNEAK,    2,      ACT_CTX_FIRST_TURN,  60,     6  },
    /* ACTION_STUN     */ { ACTION_STUN,     SKILL_BLADES,   2,      ACT_CTX_CAN_STUN,    42,     0  },
    /* ACTION_CALM     */ { ACTION_CALM,     SKILL_DIPLOMACY,2,      0,                   22,     0  },
    /* ACTION_HIDE     */ { ACTION_HIDE,     SKILL_SNEAK,    3,      0,                   20,     0  },
    /* ACTION_EXECUTE  */ { ACTION_EXECUTE,  SKILL_BLADES,   4,      ACT_CTX_EXECUTABLE,  78,     15 },
};

static const char *actionName(ActionId id) {
    switch (id) {
        case ACTION_ATTACK:   return "Attack";
        case ACTION_STRONG:   return "Strong Attack";
        case ACTION_HEAL:     return "Heal";
        case ACTION_DEFEND:   return "Defend";
        case ACTION_DISARM:   return "Disarm";
        case ACTION_BACKSTAB: return "Backstab";
        case ACTION_STUN:     return "Stun";
        case ACTION_CALM:     return "Calm";
        case ACTION_HIDE:     return "Hide";
        case ACTION_EXECUTE:  return "Execute";
        default:              return "???";
    }
}

/* Returns 1 if all context conditions for def are met right now. */
static int checkContext(const ActionDef *def) {
    uint8_t ctx = def->contextFlags;
    if ((ctx & ACT_CTX_FIRST_TURN)   && !combat.isFirstTurn)                         return 0;
    if ((ctx & ACT_CTX_ENEMY_WEAPON) && !(combat.enemy.flags & ENEMY_HAS_WEAPON))    return 0;
    if ((ctx & ACT_CTX_CAN_STUN)     && !(combat.enemy.flags & ENEMY_STUNNABLE))     return 0;
    if ((ctx & ACT_CTX_PLAYER_HURT)  && playerHp >= player.maxHp / 2)                return 0;
    if (ctx & ACT_CTX_EXECUTABLE) {
        if (!(combat.enemy.flags & ENEMY_EXECUTABLE))           return 0;
        if (combat.enemy.hp > combat.enemy.maxHp / 3)           return 0;
    }
    return 1;
}

/* Returns the final selection weight for def given the current enemy.
 * Extend this function when new enemy stats or mechanics are added. */
static int computeWeight(const ActionDef *def) {
    int w = def->baseWeight;
    switch (def->id) {
        case ACTION_STRONG:
            w += combat.enemy.size   * 5;
            w -= combat.enemy.speed  * 3;
            break;
        case ACTION_DISARM:
            w += combat.enemy.speed  * 3; /* faster enemies more threatening to leave armed */
            break;
        case ACTION_BACKSTAB:
            w -= combat.enemy.perception * 4;
            break;
        case ACTION_CALM:
            w += combat.enemy.intelligence * 5;
            break;
        case ACTION_HIDE:
            w -= combat.enemy.perception * 4;
            break;
        default:
            break;
    }
    return w < 1 ? 1 : w;
}

static void generateActions(void) {
    /* Build candidate pool: eligible actions with their weights. */
    typedef struct { int defIdx; int weight; } Candidate;
    Candidate pool[ACTION_COUNT];
    int poolSize = 0;

    for (int i = 0; i < ACTION_COUNT; i++) {
        const ActionDef *def = &actionDefs[i];
        /* Skill check */
        if (def->requiredSkill != 0xFF &&
            player.skills[def->requiredSkill] < def->requiredLevel)
            continue;
        /* Context check */
        if (!checkContext(def)) continue;
        pool[poolSize].defIdx = i;
        pool[poolSize].weight = computeWeight(def);
        poolSize++;
    }

    /* Weighted random pick without replacement — at most 4 actions. */
    combat.actionCount = 0;
    int picks = poolSize < 4 ? poolSize : 4;
    for (int pick = 0; pick < picks; pick++) {
        int total = 0;
        for (int i = 0; i < poolSize; i++) total += pool[i].weight;
        int r = rand() % total;
        int acc = 0;
        for (int i = 0; i < poolSize; i++) {
            acc += pool[i].weight;
            if (r < acc) {
                const ActionDef *chosen = &actionDefs[pool[i].defIdx];
                combat.actions[combat.actionCount++] = (Action){ chosen->id, chosen->power };
                pool[i] = pool[--poolSize]; /* remove from pool */
                break;
            }
        }
    }
    combat.selectedIndex = 0;
}

void startCombat(void) {
    combat.enemy.hp           = 20;
    combat.enemy.maxHp        = 20;
    combat.enemy.attack       = 4;
    combat.enemy.defense      = 1;
    combat.enemy.size         = 2;
    combat.enemy.speed        = 2;
    combat.enemy.intelligence = 1;
    combat.enemy.perception   = 2;
    combat.enemy.flags        = ENEMY_HAS_WEAPON | ENEMY_EXECUTABLE | ENEMY_STUNNABLE | ENEMY_BLOCKABLE;
    combat.isFirstTurn        = 1;
    combat.skipEnemyAttack    = 0;
    generateActions();
    state = STATE_COMBAT;
}

static void performPlayerAction(void) {
    Action *a = &combat.actions[combat.selectedIndex];
    combat.skipEnemyAttack = 0;

    switch (a->type) {
        case ACTION_ATTACK:
            combat.enemy.hp -= getAttack();
            break;

        case ACTION_STRONG:
            combat.enemy.hp -= getAttack() + a->power + player.skills[SKILL_BLADES];
            break;

        case ACTION_HEAL:
            playerHp += a->power + player.skills[SKILL_SURVIVAL];
            if (playerHp > player.maxHp) playerHp = player.maxHp;
            break;

        case ACTION_DEFEND:
            /* Damage reduction applied below in enemy counter-attack */
            break;

        case ACTION_DISARM:
            combat.enemy.flags  &= ~ENEMY_HAS_WEAPON;
            combat.enemy.attack  = combat.enemy.attack > 1 ? combat.enemy.attack / 2 : 1;
            break;

        case ACTION_BACKSTAB:
            combat.enemy.hp    -= getAttack() + a->power + player.skills[SKILL_SNEAK];
            combat.skipEnemyAttack = 1;
            break;

        case ACTION_STUN:
            combat.skipEnemyAttack = 1;
            /* Remove stunnable flag so Stun won't appear again */
            combat.enemy.flags &= ~ENEMY_STUNNABLE;
            break;

        case ACTION_CALM:
            combat.enemy.attack  = combat.enemy.attack > 1 ? combat.enemy.attack / 2 : 1;
            /* Remove calm option once used */
            combat.enemy.intelligence = 0;
            break;

        case ACTION_HIDE:
            combat.skipEnemyAttack = (player.skills[SKILL_SNEAK] > combat.enemy.perception);
            break;

        case ACTION_EXECUTE:
            combat.enemy.hp -= getAttack() * 2 + a->power + player.skills[SKILL_BLADES];
            break;

        default:
            break;
    }

    /* Enemy counter-attack */
    if (combat.enemy.hp > 0 && !combat.skipEnemyAttack) {
        int dmg = combat.enemy.attack;
        if (a->type == ACTION_DEFEND)
            dmg = dmg / 2 + 1;
        playerHp -= dmg;
    }

    if (playerHp <= 0 || combat.enemy.hp <= 0) {
        state = STATE_WORLD;
        return;
    }

    combat.isFirstTurn = 0;
    generateActions();
}

void handleCombatInput(int key) {
    switch (key) {
        case VK_UP:
            combat.selectedIndex--;
            if (combat.selectedIndex < 0) combat.selectedIndex = combat.actionCount - 1;
            break;
        case VK_DOWN:
            combat.selectedIndex = (combat.selectedIndex + 1) % combat.actionCount;
            break;
        case VK_RETURN: performPlayerAction(); break;
        case VK_ESCAPE: state = STATE_WORLD;  break;
    }
}

void renderCombat(void) {
    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(20, 10, 10));

    char buf[40];
    int x = 60, y = 60;
    const int lineH = 20;

    snprintf(buf, sizeof(buf), "Enemy HP: %d / %d", combat.enemy.hp, combat.enemy.maxHp);
    drawText(x, y, buf, rgb(220, 80, 80), 2); y += lineH + 4;

    snprintf(buf, sizeof(buf), "Your HP:  %d / %d", playerHp, player.maxHp);
    drawText(x, y, buf, rgb(80, 220, 80), 2); y += lineH + 16;

    for (int i = 0; i < combat.actionCount; i++) {
        int sel = (i == combat.selectedIndex);
        uint32_t color = sel ? rgb(255, 255, 100) : rgb(180, 180, 180);
        snprintf(buf, sizeof(buf), "%s%s",
            sel ? "> " : "  ",
            actionName(combat.actions[i].type));
        drawText(x, y, buf, color, 2);
        y += lineH;
    }
}
