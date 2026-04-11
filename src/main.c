#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pak.h"
#include "player.h"
#include "items.h"
#include "gfx.h"
#include "game.h"
#include "enemies.h"
#include "combat.h"
#include "town.h"
#include "world.h"
#include "skills.h"
#include "quests.h"
#include "mainmenu.h"
#include "pausemenu.h"
#include "save.h"

/* State machine */
GameState state = STATE_MAIN_MENU;

/* --- Input --- */

static int  g_pendingKey  = 0;
static char g_pendingChar = 0;
static DWORD g_savedNotifyEnd = 0;
#define SAVED_NOTIFY_MS 2000


static void handleInventoryInput(int key) {
    switch (key) {
        case VK_UP:
            inventory.selected--;
            if (inventory.selected < 0)
                inventory.selected = inventory.count > 0 ? inventory.count - 1 : 0;
            break;
        case VK_DOWN:
            if (inventory.count > 0)
                inventory.selected = (inventory.selected + 1) % inventory.count;
            break;
        case VK_RETURN:
            if (inventory.count > 0)
                useOrEquipItem(inventory.selected);
            break;
        case VK_ESCAPE:
            state = STATE_WORLD;
            break;
    }
}

static GameState g_charSheetReturn;

static void openCharSheet(GameState from) {
    g_charSheetReturn = from;
    state = STATE_CHAR_SHEET;
}

static void handleCharSheetInput(int key) {
    if (key == VK_ESCAPE || key == 'C')
        state = g_charSheetReturn;
}

static void handleDungeonInput(int key) { if (key == VK_ESCAPE) state = STATE_WORLD; }

/* --- Render --- */

static void renderInventory(void) {
    int x = 60, y = 55;
    const int lineH = 22;
    char buf[48];
    int i;

    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(10, 20, 50));
    drawText(x, y, "INVENTORY", rgb(220, 220, 255), 2);
    y += lineH + 4;

    if (inventory.count == 0) {
        drawText(x, y, "  (empty)", rgb(120, 120, 120), 2);
    } else {
        for (i = 0; i < inventory.count; i++) {
            uint8_t id     = inventory.items[i];
            int     sel    = (i == inventory.selected);
            uint32_t color = sel ? rgb(255, 255, 100) : rgb(180, 180, 180);

            snprintf(buf, sizeof(buf), "%s%-10s", sel ? "> " : "  ", itemName(id));
            if (id == player.weaponId || id == player.armorId) {
                size_t len = strlen(buf);
                buf[len] = ' '; buf[len+1] = '('; buf[len+2] = 'E'; buf[len+3] = ')'; buf[len+4] = '\0';
            }
            drawText(x, y, buf, color, 2);
            y += lineH;
        }
    }

    /* Divider */
    int divY = y + 6;
    fillRect(x, divY, gfxWidth - 120, 1, rgb(60, 60, 100));
    y = divY + 10;

    /* Stats affected by selected item */
    if (inventory.count > 0) {
        uint8_t id       = inventory.items[inventory.selected];
        const ItemDef *d = itemGetDef(id);

        drawText(x, y, itemDesc(id), rgb(160, 200, 255), 2);
        y += lineH + 2;

        if (d) {
            int curAtk = getAttack(), curDef = getDefense();
            int curInt = getIntelligence(), curPer = getPerception(), curSta = getStamina();
            int preAtk = curAtk, preDef = curDef, preHp = player.hp;
            int preInt = curInt, prePer = curPer, preSta = curSta;

            if (d->type == ITEM_WEAPON) preAtk = player.attack + d->attackBonus;
            else if (d->type == ITEM_ARMOR) preDef = player.defense + d->defenseBonus;
            if (d->flags & ITEM_FLAG_HEAL) { preHp += 10; if (preHp > player.maxHp) preHp = player.maxHp; }
            preInt += d->intelligenceBonus;
            prePer += d->perceptionBonus;
            preSta += d->staminaBonus;

            #define STAT_ROW(label, cur, pre, col) \
                if ((pre) != (cur)) snprintf(buf, sizeof(buf), label ": %d -> %d", cur, pre); \
                else                snprintf(buf, sizeof(buf), label ": %d", cur); \
                drawText(x, y, buf, col, 2); y += lineH;

            if (d->attackBonus      || d->type == ITEM_WEAPON)    { STAT_ROW("ATK", curAtk, preAtk, rgb(220, 100, 100)) }
            if (d->defenseBonus     || d->type == ITEM_ARMOR)     { STAT_ROW("DEF", curDef, preDef, rgb(100, 160, 220)) }
            if (d->flags & ITEM_FLAG_HEAL)                        { STAT_ROW("HP",  player.hp, preHp, rgb(100, 220, 100)) }
            if (d->intelligenceBonus)                             { STAT_ROW("INT", curInt, preInt, rgb(180, 100, 220)) }
            if (d->perceptionBonus)                               { STAT_ROW("PER", curPer, prePer, rgb(220, 180, 100)) }
            if (d->staminaBonus)                                  { STAT_ROW("STA", curSta, preSta, rgb(100, 220, 180)) }
            #undef STAT_ROW
        }
    }

    /* Gold — always shown */
    if(player.gold <=1)
        snprintf(buf, sizeof(buf), "Solmark: %d", player.gold);
    else
        snprintf(buf, sizeof(buf), "Solmarks: %d", player.gold);
    drawText(x, gfxHeight - 80, buf, rgb(255, 215, 0), 2);
}

static void renderCharSheet(void) {
    const int x = 60, lineH = 22;
    int y = 55;
    char buf[48];

    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(10, 20, 50));
    drawText(x, y, "CHARACTER", rgb(220, 220, 255), 2);
    y += lineH + 8;

    #define CSTAT(label, val, col) \
        snprintf(buf, sizeof(buf), label ": %d", val); \
        drawText(x, y, buf, col, 2); y += lineH;

    CSTAT("LVL", player.level,       rgb(220, 200, 100))
    snprintf(buf, sizeof(buf), "XP : %d / %d", player.xp, xpToNext());
    drawText(x, y, buf, rgb(180, 160, 80), 2); y += lineH;
    y += 6;
    snprintf(buf, sizeof(buf), "HP : %d / %d", player.hp, player.maxHp);
    drawText(x, y, buf, rgb(100, 220, 100), 2); y += lineH;
    CSTAT("ATK", getAttack(),         rgb(220, 100, 100))
    CSTAT("DEF", getDefense(),        rgb(100, 160, 220))
    y += 6;
    CSTAT("INT", getIntelligence(),   rgb(180, 100, 220))
    CSTAT("PER", getPerception(),     rgb(220, 180, 100))
    CSTAT("STA", getStamina(),        rgb(100, 220, 180))
    #undef CSTAT

    drawText(x, gfxHeight - 80, "C / ESC  to close", rgb(80, 80, 80), 1);
}

static void handleDeathInput(int key) {

    if (key == VK_RETURN || key == VK_ESCAPE) 
    {
        returnToTown();
        startTown();
    }
}

static void renderDeath(void) {
    const int cx = gfxWidth  / 2;
    const int cy = gfxHeight / 2;
    const int scale = 2;
    const int lineH = 8 * scale + 6;

    fillRect(0, 0, gfxWidth, gfxHeight, rgb(15, 5, 5));
    fillRect(30, cy - 60, gfxWidth - 60, 120, rgb(30, 10, 10));
    fillRect(30, cy - 60, gfxWidth - 60, 2, rgb(120, 40, 40));
    fillRect(30, cy + 58, gfxWidth - 60, 2, rgb(120, 40, 40));

    drawText(cx - 120, cy - 48, "YOU HAVE FALLEN", rgb(200, 60, 60), scale);
    drawText(cx - 272, cy - 48 + lineH,
        "You have been knocked unconscious.", rgb(180, 160, 140), scale);
    drawText(cx - 288, cy - 48 + lineH * 2,
        "You wake up slowly at the healer in", rgb(180, 160, 140), scale);
    drawText(cx - 128, cy - 48 + lineH * 3,
        "the nearby town.", rgb(180, 160, 140), scale);
    drawText(cx - 92, cy - 48 + lineH * 4 + 4,
        "Press Enter to continue", rgb(100, 80, 60), 1);
}

static void renderDungeon(void) {
    /* Placeholder — dungeon system coming soon */
    fillRect(40, 40, gfxWidth - 80, gfxHeight - 80, rgb(80, 0, 80));
}

/* --- Win32 --- */

static int g_running = 1;
HWND       g_hwnd;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_KEYDOWN:
            g_pendingKey = (int)wp;
            return 0;
        case WM_CHAR:
            g_pendingChar = (char)wp;
            return 0;
        case WM_SIZE:
            gfxResize((int)LOWORD(lp), (int)HIWORD(lp));
            worldUpdateCamera();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            g_running = 0;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR cmdLine, int nCmdShow) {
    (void)hPrev; (void)cmdLine;

    srand((unsigned int)GetTickCount());

    if (!pakOpen("data.pak")) return 1;

    PakData playerData  = pakRead("assets/player.dat");
    PakData itemData    = pakRead("assets/items.dat");
    PakData enemyData   = pakRead("assets/enemies.dat");
    PakData dialogData  = pakRead("assets/dialog.dat");
    PakData questData   = pakRead("assets/quests.dat");

    if (!loadPlayer(playerData)) { pakClose(); return 1; }
    loadItems(itemData);     /* optional — falls back to builtins if not in pak */
    loadEnemies(enemyData);  /* optional — falls back to builtins if not in pak */
    loadDialogs(dialogData); /* optional — falls back to builtins if not in pak */
    loadQuests(questData);   /* optional — no quests if absent */
    free(playerData.data);
    free(itemData.data);
    free(enemyData.data);
    free(dialogData.data);
    free(questData.data);

    const int screenW = 640;
    const int screenH = 480;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "TinyHero";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    RECT wr = {0, 0, screenW, screenH};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowA(
        "TinyHero", "Tiny Hero",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL
    );

    gfxInit(g_hwnd, screenW, screenH);

    if (!worldLoadNamed("assets/map1.bin")) { pakClose(); gfxShutdown(); return 1; }
    /* pak stays open for dynamic map loading during gameplay */

    ShowWindow(g_hwnd, nCmdShow);

    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_pendingKey) {
            /* F5 quick-saves from any in-game state */
            if (g_pendingKey == VK_F5 && state != STATE_MAIN_MENU) {
                if (saveGameNew())
                    g_savedNotifyEnd = GetTickCount() + SAVED_NOTIFY_MS;
            /* Escape opens pause from the main gameplay states */
            } else if (g_pendingKey == VK_ESCAPE &&
                       (state == STATE_WORLD || state == STATE_COMBAT || state == STATE_TOWN)) {
                enterPauseMenu(state);
            /* Pause menu gets both key and char for the save-name form */
            } else if (state == STATE_PAUSE_MENU) {
                handlePauseMenuInput(g_pendingKey, g_pendingChar);
            /* P is a global hotkey — opens skills from world, closes from skills */
            } else if (g_pendingKey == 'P' && (state == STATE_WORLD || state == STATE_SKILLS)) {
                state = (state == STATE_SKILLS) ? STATE_WORLD : STATE_SKILLS;
            /* J toggles the quest log from any non-combat state */
            } else if (g_pendingKey == 'J' && state != STATE_COMBAT) {
                if (state == STATE_QUEST_LOG) {
                    state = questLogSt.returnState;
                } else {
                    questLogSt.returnState = state;
                    questLogSt.sel = 0;
                    state = STATE_QUEST_LOG;
                }
            /* C toggles the character sheet from any non-combat state */
            } else if (g_pendingKey == 'C' && state != STATE_COMBAT && state != STATE_MAIN_MENU && state != STATE_LOADING) {
                if (state == STATE_CHAR_SHEET)
                    state = g_charSheetReturn;
                else
                    openCharSheet(state);
            } else {
                switch (state) {
                    case STATE_WORLD:   handleWorldInput(g_pendingKey);   break;
                    case STATE_COMBAT:  handleCombatInput(g_pendingKey);  break;
                    case STATE_MAIN_MENU: handleMainMenuInput(g_pendingKey); break;
                    case STATE_INVENTORY: handleInventoryInput(g_pendingKey); break;
                    case STATE_SKILLS:  handleSkillsInput(g_pendingKey);  break;
                    case STATE_DIALOG:  handleDialogInput(g_pendingKey);  break;
                    case STATE_DUNGEON:   handleDungeonInput(g_pendingKey);   break;
                    case STATE_TOWN:     handleTownInput(g_pendingKey);      break;
                    case STATE_QUEST_LOG:  handleQuestLogInput(g_pendingKey); break;
                    case STATE_DEATH:      handleDeathInput(g_pendingKey);   break;
                    case STATE_CHAR_SHEET: handleCharSheetInput(g_pendingKey); break;
                    case STATE_PAUSE_MENU: break; /* handled above */
                    case STATE_LOADING: break;
                }
            }
            g_pendingKey  = 0;
            g_pendingChar = 0;
        }

        if (state == STATE_LOADING) updateLoading();

        clearScreen();
        switch (state) {
            case STATE_WORLD:   renderWorld();   break;
            case STATE_COMBAT:  renderCombat();  break;
            case STATE_LOADING:   renderLoading();   break;
            case STATE_MAIN_MENU: renderMainMenu(); break;
            case STATE_INVENTORY: renderInventory(); break;
            case STATE_SKILLS:  renderSkills();  break;
            case STATE_DIALOG:  renderDialog();  break;
            case STATE_DUNGEON:   renderDungeon();   break;
            case STATE_TOWN:     renderTown();      break;
            case STATE_QUEST_LOG: renderQuestLog(); break;
            case STATE_DEATH:      renderDeath();      break;
            case STATE_PAUSE_MENU: renderPauseMenu();  break;
            case STATE_CHAR_SHEET: renderCharSheet();  break;
        }

        if (g_savedNotifyEnd && GetTickCount() < g_savedNotifyEnd)
            drawText(gfxWidth - 72, 8, "SAVED!", rgb(100, 255, 100), 1);
        renderQuestNotifications();

        gfxPresent(g_hwnd);
        Sleep(16); //consider
    }

    pakClose();
    gfxShutdown();
    return 0;
}
