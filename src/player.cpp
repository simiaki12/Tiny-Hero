#include "player.h"
#include <cstring>

PlayerData player;

bool loadPlayer(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(PlayerData)) return false;
    memcpy(&player, data.data(), sizeof(PlayerData));
    return true;
}
