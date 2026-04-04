#pragma once
#include <vector>
#include <cstdint>

struct PlayerData {
    uint8_t maxHp;
    uint8_t attack;
    uint8_t defense;

    uint8_t weaponId;
    uint8_t armorId;

    uint8_t abilityCount;
    uint8_t abilities[8];
};

extern PlayerData player;

bool loadPlayer(const std::vector<uint8_t>& data);
