#include "items.h"
#include "player.h"
#include "quests.h"
#include <stdio.h>
#include <string.h>

ItemDef   itemDefs[64];
int       itemDefCount = 0;

Inventory inventory = {
    {0, 1, 2},
    3,
    0
};

static const ItemDef builtinDefs[] = {
    { "Iron Sword",     ITEM_WEAPON,      3,  0, 0              },
    { "Leather Armor",  ITEM_ARMOR,       0,  2, 0              },
    { "Health Potion",  ITEM_CONSUMABLE,  0,  0, ITEM_FLAG_HEAL },
};
#define BUILTIN_COUNT (int)(sizeof(builtinDefs)/sizeof(builtinDefs[0]))

/* Format: [1 count][N × sizeof(ItemDef)] */
int loadItems(PakData data) {
    if (!data.data || data.size < 1) return 0;
    uint8_t n = ((const uint8_t *)data.data)[0];
    if (n > 64) n = 64;
    uint32_t expected = 1 + (uint32_t)n * sizeof(ItemDef);
    if (data.size < expected) return 0;
    memcpy(itemDefs, (const uint8_t *)data.data + 1, (size_t)n * sizeof(ItemDef));
    itemDefCount = n;
    return n;
}

const ItemDef *itemGetDef(uint8_t id) {
    if (itemDefCount > 0 && id < (uint8_t)itemDefCount) return &itemDefs[id];
    if ((int)id < BUILTIN_COUNT) return &builtinDefs[id];
    return NULL;
}

const char *itemName(uint8_t id) {
    const ItemDef *d = itemGetDef(id);
    return d ? d->name : "???";
}

const char *itemDesc(uint8_t id) {
    static char buf[32];
    const ItemDef *d = itemGetDef(id);
    if (!d) return "";
    switch (d->type) {
        case ITEM_WEAPON:
            snprintf(buf, sizeof(buf), "%+d ATK", d->attackBonus);
            return buf;
        case ITEM_ARMOR:
            snprintf(buf, sizeof(buf), "%+d DEF", d->defenseBonus);
            return buf;
        case ITEM_CONSUMABLE:
            if (d->flags & ITEM_FLAG_HEAL)        return "Restores HP";
            if (d->flags & ITEM_FLAG_BUFF_ATTACK)  return "Boosts ATK";
            return "Consumable";
        default:
            return "";
    }
}

int getAttack(void) {
    int atk = player.attack;
    if (player.weaponId != ITEM_UNEQUIPPED) {
        const ItemDef *d = itemGetDef(player.weaponId);
        if (d) atk += d->attackBonus;
    }
    return atk;
}

int getDefense(void) {
    int def = player.defense;
    if (player.armorId != ITEM_UNEQUIPPED) {
        const ItemDef *d = itemGetDef(player.armorId);
        if (d) def += d->defenseBonus;
    }
    return def;
}

void getPreviewStats(uint8_t id, int *atkOut, int *defOut, int *hpOut) {
    const ItemDef *d = itemGetDef(id);
    *atkOut = getAttack();
    *defOut = getDefense();
    *hpOut  = player.hp;
    if (!d) return;
    if (d->type == ITEM_WEAPON)
        *atkOut = player.attack + d->attackBonus;
    else if (d->type == ITEM_ARMOR)
        *defOut = player.defense + d->defenseBonus;
    else if ((d->type == ITEM_CONSUMABLE) && (d->flags & ITEM_FLAG_HEAL)) {
        *hpOut += 10;
        if (*hpOut > player.maxHp) *hpOut = player.maxHp;
    }
}

int addItem(uint8_t id) {
    if (inventory.count >= 16) return 0;
    inventory.items[inventory.count++] = id;
    questOnItemGained(id);
    return 1;
}

void removeItem(int index) {
    int i;
    for (i = index; i < inventory.count - 1; i++)
        inventory.items[i] = inventory.items[i + 1];
    inventory.count--;
    if (inventory.selected >= (int8_t)inventory.count)
        inventory.selected = inventory.count - 1;
}

void useOrEquipItem(int index) {
    uint8_t id = inventory.items[index];
    const ItemDef *d = itemGetDef(id);
    if (!d) return;
    if (d->type == ITEM_WEAPON) {
        player.weaponId = id;
    } else if (d->type == ITEM_ARMOR) {
        player.armorId = id;
    } else if (d->type == ITEM_CONSUMABLE) {
        if (d->flags & ITEM_FLAG_HEAL) {
            int newHp = (int)player.hp + 10;
            player.hp = (uint8_t)(newHp > player.maxHp ? player.maxHp : newHp);
        }
        removeItem(index);
    }
}
