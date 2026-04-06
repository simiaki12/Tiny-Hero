#include "items.h"
#include "player.h"
#include <string.h>

ItemDef   itemDefs[64];
int       itemDefCount = 0;

Inventory inventory = {
    {0, 1, 2},
    3,
    0
};

/* Hardcoded until items.dat is added to the pak */
static const ItemDef builtinDefs[] = {
    { ITEM_WEAPON,     3, 0, 0,              {0,0,0,0} }, /* 0: Sword  */
    { ITEM_ARMOR,      0, 2, 0,              {0,0,0,0} }, /* 1: Armor  */
    { ITEM_CONSUMABLE, 0, 0, ITEM_FLAG_HEAL, {0,0,0,0} }, /* 2: Potion */
};
#define BUILTIN_COUNT (int)(sizeof(builtinDefs)/sizeof(builtinDefs[0]))

int loadItems(PakData data) {
    if (!data.data) return 0;
    int n = (int)(data.size / sizeof(ItemDef));
    if (n > 64) n = 64;
    memcpy(itemDefs, data.data, (size_t)n * sizeof(ItemDef));
    itemDefCount = n;
    return n;
}

const ItemDef *itemGetDef(uint8_t id) {
    if (itemDefCount > 0 && id < (uint8_t)itemDefCount) return &itemDefs[id];
    if ((int)id < BUILTIN_COUNT) return &builtinDefs[id];
    return NULL;
}

const char *itemName(uint8_t id) {
    switch (id) {
        case 0: return "Sword";
        case 1: return "Armor";
        case 2: return "Potion";
        default: return "???";
    }
}

const char *itemDesc(uint8_t id) {
    switch (id) {
        case 0: return "+3 ATK";
        case 1: return "+2 DEF";
        case 2: return "Restores 10 HP";
        default: return "";
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
    *hpOut  = playerHp;
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
            playerHp += 10;
            if (playerHp > player.maxHp) playerHp = player.maxHp;
        }
        removeItem(index);
    }

}
