#include "player.h"
#include "items.h"
#include "game.h"
#include <string.h>

PlayerData player;

int loadPlayer(PakData data) {
    if (data.size < 3) return 0;
    memset(&player, 0, sizeof(player));
    memset(player.equipped, ITEM_UNEQUIPPED, EQUIP_SLOTS);
    uint32_t n = data.size < sizeof(player) ? data.size : (uint32_t)sizeof(player);
    memcpy(&player, data.data, n);
    if (!player.level) player.level = 1;
    if (!player.hp)    player.hp    = player.maxHp;
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
        player.hp = player.maxHp;
        gained++;
    }
    player.xp = (uint16_t)xp;
    return gained;
}

void enterDeath(void) {
    player.xp = 0;        /* lose XP progress within current level — level is kept */
    player.hp  = player.maxHp; /* healed by the town healer */
    state     = STATE_DEATH;
}
