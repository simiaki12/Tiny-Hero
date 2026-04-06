#pragma once
#include <stdint.h>
#include "pak.h"
#include "skills.h"

typedef struct {
    uint8_t maxHp;
    uint8_t attack;
    uint8_t defense;
    uint8_t weaponId;
    uint8_t armorId;
    uint8_t skills[SKILL_MAX];
} PlayerData;

extern PlayerData player;
extern int playerHp;

int loadPlayer(PakData data);
