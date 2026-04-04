#pragma once

typedef enum { STATE_WORLD, STATE_COMBAT, STATE_MENU, STATE_DIALOG, STATE_DUNGEON, STATE_TOWN } GameState;
extern GameState state;
