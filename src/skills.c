#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "skills.h"
#include "game.h"
#include "player.h"
#include "gfx.h"

const char *skillName(int skill) {
    switch (skill) {
        case SKILL_BLADES:    return "Blades";
        case SKILL_SNEAK:     return "Sneak";
        case SKILL_MAGIC:     return "Magic";
        case SKILL_DIPLOMACY: return "Diplomacy";
        case SKILL_SURVIVAL:  return "Survival";
        case SKILL_ARCHERY:   return "Archery";
        default:              return "???";
    }
}

void handleSkillsInput(int key) {
    if (key == VK_ESCAPE || key == 'P') state = STATE_WORLD;
}

void renderSkills(void) {
    int x = 60, y = 55;
    const int lineH = 22;
    char buf[32];

    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(10, 30, 10));
    drawText(x, y, "SKILLS", rgb(180, 220, 180), 2);
    y += lineH + 8;

    for (int i = 0; i < SKILL_COUNT; i++) {
        uint8_t val = player.skills[i];
        snprintf(buf, sizeof(buf), "%-12s %2d", skillName(i), val);
        drawText(x, y, buf, val > 0 ? rgb(160, 220, 160) : rgb(80, 100, 80), 2);
        y += lineH;
    }

    drawText(x, gfxHeight - 80, "P / ESC  close", rgb(60, 80, 60), 1);
}
