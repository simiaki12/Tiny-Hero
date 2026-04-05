#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Must match src/player.h */
typedef struct {
    uint8_t maxHp;
    uint8_t attack;
    uint8_t defense;
    uint8_t weaponId;
    uint8_t armorId;
    uint8_t abilityCount;
    uint8_t abilities[8];
    uint8_t skills[16];
} PlayerData;

static PlayerData p;
static const char *outfile = "assets/player.dat";

static void load(void) {
    FILE *f = fopen(outfile, "rb");
    if (!f) {
        memset(&p, 0, sizeof(p));
        p.maxHp = 20; p.attack = 5; p.defense = 3;
        p.weaponId = 0xFF; p.armorId = 0xFF; /* unequipped */
        return;
    }
    (void)fread(&p, 1, sizeof(p), f);
    fclose(f);
}

static void save(void) {
    FILE *f = fopen(outfile, "wb");
    if (!f) return;
    fwrite(&p, 1, sizeof(p), f);
    fclose(f);
}

int main(void) {
    load();

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    const char *names[] = {
        "Max HP", "Attack", "Defense", "Weapon ID", "Armor ID",
        "Ability Count",
        "Ability 0", "Ability 1", "Ability 2", "Ability 3",
        "Ability 4", "Ability 5", "Ability 6", "Ability 7",
        /* skills — keep in sync with SKILL_* in skills.h */
        "Blades", "Sneak", "Magic", "Diplomacy", "Survival", "Archery",
        "Skill 6", "Skill 7", "Skill 8", "Skill 9",
        "Skill 10", "Skill 11", "Skill 12", "Skill 13", "Skill 14", "Skill 15"
    };
    uint8_t *fields[] = {
        &p.maxHp, &p.attack, &p.defense, &p.weaponId, &p.armorId,
        &p.abilityCount,
        &p.abilities[0], &p.abilities[1], &p.abilities[2], &p.abilities[3],
        &p.abilities[4], &p.abilities[5], &p.abilities[6], &p.abilities[7],
        &p.skills[0],  &p.skills[1],  &p.skills[2],  &p.skills[3],
        &p.skills[4],  &p.skills[5],  &p.skills[6],  &p.skills[7],
        &p.skills[8],  &p.skills[9],  &p.skills[10], &p.skills[11],
        &p.skills[12], &p.skills[13], &p.skills[14], &p.skills[15]
    };
    uint8_t maxVals[] = {
        255, 255, 255, 255, 255, 8,
        255, 255, 255, 255, 255, 255, 255, 255,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
    };
    int nFields = 30;
    int sel     = 0;
    int dirty   = 0;
    int running = 1;

    while (running) {
        clear();
        mvprintw(0, 0, "PLAYER EDITOR  [%s]", dirty ? "unsaved" : "saved");
        mvprintw(1, 0, "Up/Down=select  +/-=change  S=save  Q=quit");

        for (int i = 0; i < nFields; i++) {
            int active = (i <= 5) || (i >= 14) || ((i - 6) < p.abilityCount);
            if (i == sel) attron(A_REVERSE);
            if (!active) attron(A_DIM);
            mvprintw(i + 3, 4, "%-16s %3d%s", names[i], *fields[i],
                     (i > 5 && !active) ? "  (inactive)" : "");
            if (!active) attroff(A_DIM);
            if (i == sel) attroff(A_REVERSE);
        }

        refresh();

        int ch = getch();
        switch (ch) {
            case KEY_UP:
                if (sel > 0) sel--;
                break;
            case KEY_DOWN:
                if (sel < nFields - 1) sel++;
                break;
            case '+': case '=':
                if (*fields[sel] < maxVals[sel]) { (*fields[sel])++; dirty = 1; }
                break;
            case '-':
                if (*fields[sel] > 0) { (*fields[sel])--; dirty = 1; }
                break;
            case 's': case 'S':
                save(); dirty = 0;
                break;
            case 'q': case 'Q':
                running = 0;
                break;
        }
    }

    endwin();

    if (dirty) {
        printf("Unsaved changes. Save? (y/n): ");
        fflush(stdout);
        int ch = getchar();
        if (ch == 'y' || ch == 'Y') save();
    }

    return 0;
}
