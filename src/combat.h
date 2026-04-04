#pragma once
#include <stdint.h>

#define ABILITY_HEAL   1
#define ABILITY_STRONG 2

typedef enum { ACTION_ATTACK, ACTION_STRONG, ACTION_HEAL } ActionType;
typedef struct { ActionType type; uint8_t power; } Action;
typedef struct { int hp; uint8_t attack; uint8_t defense; } Enemy;
typedef struct {
    Enemy enemy;
    Action actions[4];
    int actionCount;
    int selectedIndex;
} CombatState;

extern CombatState combat;

void startCombat(void);
void handleCombatInput(int key);
void renderCombat(void);
