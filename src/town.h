#pragma once
#include <stdint.h>
#include "game.h"

#define DIALOG_MAX_LINES 8
#define DIALOG_LINE_LEN  40

typedef struct {
    uint8_t lineCount;
    char    lines[DIALOG_MAX_LINES][DIALOG_LINE_LEN];
} Dialog;

typedef struct {
    int       dialogId;
    int       currentLine;
    GameState returnState;
} DialogState;

typedef struct {
    int selected;
} TownState;

extern DialogState dialogSt;
extern TownState   townSt;

void startTown(void);
void startDialog(int id, GameState returnTo);
void handleTownInput(int key);
void handleDialogInput(int key);
void renderTown(void);
void renderDialog(void);
