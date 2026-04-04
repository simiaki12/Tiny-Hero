#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "town.h"
#include "game.h"
#include "player.h"
#include "gfx.h"

DialogState dialogSt;
TownState   townSt;

/* Hardcoded dialogs — will move to dialog.dat in pak later */
static const Dialog dialogs[] = {
    { 2, {
        "Welcome, traveler.",
        "Stay safe out there."
    }},
    { 3, {
        "The dungeon to the east is dangerous.",
        "Many have entered.",
        "Few have returned."
    }},
};
#define DIALOG_COUNT (int)(sizeof(dialogs)/sizeof(dialogs[0]))

static const char *townOptions[] = { "Talk", "Rest", "Leave" };
#define TOWN_OPTION_COUNT 3

void startTown(void) {
    townSt.selected = 0;
    state = STATE_TOWN;
}

void startDialog(int id, GameState returnTo) {
    if (id < 0 || id >= DIALOG_COUNT) return;
    dialogSt.dialogId    = id;
    dialogSt.currentLine = 0;
    dialogSt.returnState = returnTo;
    state = STATE_DIALOG;
}

void handleTownInput(int key) {
    switch (key) {
        case VK_UP:
            townSt.selected--;
            if (townSt.selected < 0) townSt.selected = TOWN_OPTION_COUNT - 1;
            break;
        case VK_DOWN:
            townSt.selected = (townSt.selected + 1) % TOWN_OPTION_COUNT;
            break;
        case VK_RETURN:
            switch (townSt.selected) {
                case 0: startDialog(0, STATE_TOWN); break;
                case 1:
                    playerHp = player.maxHp;
                    break;
                case 2:
                    state = STATE_WORLD;
                    break;
            }
            break;
        case VK_ESCAPE:
            state = STATE_WORLD;
            break;
    }
}

void handleDialogInput(int key) {
    const Dialog *d = &dialogs[dialogSt.dialogId];
    if (key == VK_RETURN || key == VK_SPACE) {
        dialogSt.currentLine++;
        if (dialogSt.currentLine >= d->lineCount)
            state = dialogSt.returnState;
    } else if (key == VK_ESCAPE) {
        state = dialogSt.returnState;
    }
}

void renderTown(void) {
    int x = 60, y = 55;
    const int lineH = 24;
    int i;

    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(10, 30, 60));
    drawText(x, y, "TOWN", rgb(220, 200, 100), 2);
    y += lineH + 8;

    for (i = 0; i < TOWN_OPTION_COUNT; i++) {
        uint32_t color = (i == townSt.selected) ? rgb(255, 255, 100) : rgb(180, 180, 180);
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%s",
            (i == townSt.selected) ? "> " : "  ",
            townOptions[i]);
        drawText(x, y, buf, color, 2);
        y += lineH;
    }
}

void renderDialog(void) {
    const Dialog *d   = &dialogs[dialogSt.dialogId];
    const char   *line = d->lines[dialogSt.currentLine];

    int boxH = 70;
    int boxY = gfxHeight - boxH - 20;

    /* Background box */
    fillRect(20, boxY, gfxWidth - 40, boxH, rgb(0, 0, 40));
    /* Top border */
    fillRect(20, boxY, gfxWidth - 40, 2, rgb(140, 140, 200));

    drawText(30, boxY + 12, line, rgb(220, 220, 220), 2);

    /* Continue prompt */
    char progress[16];
    snprintf(progress, sizeof(progress), "%d/%d",
        dialogSt.currentLine + 1, d->lineCount);
    drawText(gfxWidth - 80, boxY + 48, progress, rgb(100, 100, 120), 1);
}
