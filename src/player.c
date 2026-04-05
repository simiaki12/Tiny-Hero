#include "player.h"
#include <string.h>

PlayerData player;
int playerHp = 0;

int loadPlayer(PakData data) {
    if (data.size < 5) return 0; /* need at least: maxHp, attack, defense, weaponId, armorId */
    memset(&player, 0, sizeof(player));
    uint32_t n = data.size < sizeof(player) ? data.size : (uint32_t)sizeof(player);
    memcpy(&player, data.data, n);
    return 1;
}
