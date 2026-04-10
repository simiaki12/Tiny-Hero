#pragma once
#include <stdint.h>
#include "pak.h"
#include "skills.h"

typedef struct {
    uint8_t  maxHp;
    uint8_t  attack;
    uint8_t  defense;
    uint8_t  weaponId;
    uint8_t  armorId;
    uint8_t  skills[SKILL_MAX];
    uint8_t  level;
    uint8_t  xp;
    uint8_t  skillPoints;
    uint8_t  hp;
    uint8_t  intelligence;
    uint8_t  perception;
    uint8_t  stamina;
    uint16_t gold;
} PlayerData;

extern PlayerData player;

int  loadPlayer(PakData data);
int  xpToNext(void);      /* XP needed to reach the next level */
int  awardXp(int amount);  /* returns number of levels gained */
void enterDeath(void);     /* lose XP progress, restore HP, go to STATE_DEATH */
