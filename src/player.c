#include "player.h"
#include "game.h"
#include <string.h>

PlayerData player;
int playerHp = 0;

int loadPlayer(PakData data) {
    if (data.size < 5) return 0; /* need at least: maxHp, attack, defense, weaponId, armorId */
    memset(&player, 0, sizeof(player));
    if (!player.level) player.level = 1;
    uint32_t n = data.size < sizeof(player) ? data.size : (uint32_t)sizeof(player);
    memcpy(&player, data.data, n);
    if (!player.level) player.level = 1;
    return 1;
}

int xpToNext(void) {
    return 20 + player.level * 10;
}

int awardXp(int amount) {
    int xp = (int)player.xp + amount;
    int gained = 0;
    while (xp >= xpToNext()) {
        xp -= xpToNext();
        player.level++;
        if (player.maxHp  < 250) player.maxHp  += 5;
        if (player.attack < 255) player.attack  += 1;
        if (player.defense< 255) player.defense += 1;
        if (player.skillPoints < 255) player.skillPoints++;
        playerHp = player.maxHp;
        gained++;
    }
    player.xp = (uint8_t)(xp > 255 ? 255 : xp);
    return gained;
}

void enterDeath(void) {
    player.xp = 0;        /* lose XP progress within current level — level is kept */
    playerHp  = player.maxHp; /* healed by the town healer */
    state     = STATE_DEATH;
}
