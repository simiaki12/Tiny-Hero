#pragma once
#include <stdint.h>
#include "pak.h"

typedef enum { ITEM_WEAPON, ITEM_ARMOR, ITEM_CONSUMABLE } ItemType;

#define ITEM_FLAG_HEAL        0x01
#define ITEM_FLAG_BUFF_ATTACK 0x02

#define ITEM_UNEQUIPPED 0xFF

/* 20 bytes — pak-friendly, no pointers */
typedef struct {
    char    name[16];
    uint8_t type;
    int8_t  attackBonus;
    int8_t  defenseBonus;
    uint8_t flags;
} ItemDef;

typedef struct {
    uint8_t items[16];
    uint8_t count;
    int8_t  selected;
} Inventory;

extern ItemDef   itemDefs[64];
extern int       itemDefCount;
extern Inventory inventory;

int         loadItems(PakData data);
const ItemDef *itemGetDef(uint8_t id);
const char *itemName(uint8_t id);
const char *itemDesc(uint8_t id);
int         getAttack(void);
int         getDefense(void);
void        getPreviewStats(uint8_t id, int *atkOut, int *defOut, int *hpOut);
int         addItem(uint8_t id);
void        useOrEquipItem(int index);
void        removeItem(int index);
