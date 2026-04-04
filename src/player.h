#pragma once
#include <stdint.h>
#include "pak.h"

typedef struct {
    uint8_t maxHp;
    uint8_t attack;
    uint8_t defense;
    uint8_t weaponId;
    uint8_t armorId;
    uint8_t abilityCount;
    uint8_t abilities[8];
} PlayerData;

extern PlayerData player;
extern int playerHp;

int loadPlayer(PakData data);
