#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "combat.h"
#include "game.h"
#include "player.h"
#include "items.h"
#include "gfx.h"

CombatState combat;

static int hasAbility(uint8_t id) {
    for (int i = 0; i < player.abilityCount; i++)
        if (player.abilities[i] == id) return 1;
    return 0;
}

static void generateActions(void) {
    combat.actionCount = 0;
    combat.actions[combat.actionCount++] = (Action){ACTION_ATTACK, 0};
    if (player.weaponId > 0 || hasAbility(ABILITY_STRONG))
        combat.actions[combat.actionCount++] = (Action){ACTION_STRONG, 3};
    if (playerHp < player.maxHp / 2 && hasAbility(ABILITY_HEAL))
        combat.actions[combat.actionCount++] = (Action){ACTION_HEAL, 5};
    combat.selectedIndex = 0;
}

void startCombat(void) {
    combat.enemy.hp      = 20;
    combat.enemy.attack  = 4;
    combat.enemy.defense = 1;
    generateActions();
    state = STATE_COMBAT;
}

static void performPlayerAction(void) {
    Action *a = &combat.actions[combat.selectedIndex];
    switch (a->type) {
        case ACTION_ATTACK: combat.enemy.hp -= getAttack();            break;
        case ACTION_STRONG: combat.enemy.hp -= getAttack() + a->power; break;
        case ACTION_HEAL:
            playerHp += a->power;
            if (playerHp > player.maxHp) playerHp = player.maxHp;
            break;
    }
    if (combat.enemy.hp > 0)
        playerHp -= combat.enemy.attack;
    if (playerHp <= 0 || combat.enemy.hp <= 0)
        state = STATE_WORLD;
    else
        generateActions();
}

void handleCombatInput(int key) {
    switch (key) {
        case VK_UP:
            combat.selectedIndex--;
            if (combat.selectedIndex < 0) combat.selectedIndex = combat.actionCount - 1;
            break;
        case VK_DOWN:
            combat.selectedIndex = (combat.selectedIndex + 1) % combat.actionCount;
            break;
        case VK_RETURN: performPlayerAction(); break;
        case VK_ESCAPE: state = STATE_WORLD;  break;
    }
}

void renderCombat(void) {
    static const char *actionNames[] = {"Attack", "Strong", "Heal"};

    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(20, 10, 10));

    char buf[32];
    int x = 60, y = 60;
    const int lineH = 20;

    snprintf(buf, sizeof(buf), "Enemy HP: %d", combat.enemy.hp);
    drawText(x, y, buf, rgb(220, 80, 80), 2); y += lineH + 4;

    snprintf(buf, sizeof(buf), "Your HP:  %d / %d", playerHp, player.maxHp);
    drawText(x, y, buf, rgb(80, 220, 80), 2); y += lineH + 16;

    for (int i = 0; i < combat.actionCount; i++) {
        uint32_t color = (i == combat.selectedIndex) ? rgb(255, 255, 100) : rgb(180, 180, 180);
        snprintf(buf, sizeof(buf), "%s%s",
            (i == combat.selectedIndex) ? "> " : "  ",
            actionNames[combat.actions[i].type]);
        drawText(x, y, buf, color, 2);
        y += lineH;
    }
}
