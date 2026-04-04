#include "player.h"
#include <string.h>

PlayerData player;
int playerHp = 0;

int loadPlayer(PakData data) {
    if (data.size < sizeof(PlayerData)) return 0;
    memcpy(&player, data.data, sizeof(PlayerData));
    return 1;
}
