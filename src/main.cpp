#include <SDL2/SDL.h>
#include <vector>
#include <cstdint>
#include <cstdio>
#include "pak.h"
#include "player.h"
#include "font8x8_basic.h"

const int TILE_SIZE = 32;

// Map
int mapWidth = 0, mapHeight = 0;
std::vector<uint8_t> mapGfx; // 0=floor, 1=wall
std::vector<uint8_t> mapLoc; // 0=empty, 1=enemy, 2=town, 3=dungeon

// Player
int playerX = 2, playerY = 2;

// State machine
enum GameState { STATE_WORLD, STATE_COMBAT, STATE_MENU, STATE_DIALOG, STATE_DUNGEON };
GameState state = STATE_WORLD;

enum LocType : uint8_t {
    LOC_EMPTY   = 0,
    LOC_ENEMY   = 1,
    LOC_TOWN    = 2,
    LOC_DUNGEON = 3,
};

bool loadMap(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    mapWidth  = data[0];
    mapHeight = data[1];
    int n = mapWidth * mapHeight;
    if ((int)data.size() < 2 + n * 2) return false;
    mapGfx.assign(data.begin() + 2,         data.begin() + 2 + n);
    mapLoc.assign(data.begin() + 2 + n,     data.begin() + 2 + n * 2);
    return true;
}

// --- Font rendering ---

static void drawChar(SDL_Renderer* renderer, int x, int y, char c, int scale = 2) {
    if ((unsigned char)c >= 128) return;
    const unsigned char* glyph = font8x8_basic[(unsigned char)c];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if ((glyph[row] >> col) & 1) {
                SDL_Rect px = { x + col * scale, y + row * scale, scale, scale };
                SDL_RenderFillRect(renderer, &px);
            }
        }
    }
}

static void drawText(SDL_Renderer* renderer, int x, int y, const char* text, int scale = 2) {
    for (int i = 0; text[i]; i++)
        drawChar(renderer, x + i * 8 * scale, y, text[i], scale);
}

// --- Input handlers ---

void handleWorldInput(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN) return;

    if (event.key.keysym.sym == SDLK_i) { state = STATE_MENU; return; }

    int newX = playerX, newY = playerY;
    switch (event.key.keysym.sym) {
        case SDLK_UP:    newY--; break;
        case SDLK_DOWN:  newY++; break;
        case SDLK_LEFT:  newX--; break;
        case SDLK_RIGHT: newX++; break;
        default: return;
    }

    if (newX >= 0 && newX < mapWidth &&
        newY >= 0 && newY < mapHeight &&
        mapGfx[newY * mapWidth + newX] == 0) {
        playerX = newX;
        playerY = newY;

        if (mapLoc[newY * mapWidth + newX] == LOC_ENEMY)
            state = STATE_COMBAT;
        if (mapLoc[newY * mapWidth + newX] == LOC_TOWN)
            state = STATE_DIALOG;
        if (mapLoc[newY * mapWidth + newX] == LOC_DUNGEON)
            state = STATE_DUNGEON;
    }
}

void handleCombatInput(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
        state = STATE_WORLD;
}

void handleMenuInput(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
        state = STATE_WORLD;
}

void handleDialogInput(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
        state = STATE_WORLD;
}

void handleDungeonInput(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
        state = STATE_WORLD;
}

// --- Render handlers ---

void renderWorld(SDL_Renderer* renderer) {
    for (int y = 0; y < mapHeight; y++) {
        for (int x = 0; x < mapWidth; x++) {
            SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE };

            if (mapGfx[y * mapWidth + x] == 1) {
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // wall
            } else {
                switch ((LocType)mapLoc[y * mapWidth + x]) {
                    case LOC_ENEMY:   SDL_SetRenderDrawColor(renderer, 120,  30,  30, 255); break;
                    case LOC_TOWN:    SDL_SetRenderDrawColor(renderer, 150, 120,   0, 255); break;
                    case LOC_DUNGEON: SDL_SetRenderDrawColor(renderer,  80,   0,  80, 255); break;
                    default:          SDL_SetRenderDrawColor(renderer,   0, 100,   0, 255); break;
                }
            }
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    SDL_Rect playerRect = { playerX * TILE_SIZE, playerY * TILE_SIZE, TILE_SIZE, TILE_SIZE };
    SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
    SDL_RenderFillRect(renderer, &playerRect);
}

void renderCombat(SDL_Renderer* renderer) {
    // Placeholder — combat system coming soon
    SDL_Rect box = { 40, 40, mapWidth * TILE_SIZE - 80, mapHeight * TILE_SIZE - 80 };
    SDL_SetRenderDrawColor(renderer, 80, 20, 20, 255);
    SDL_RenderFillRect(renderer, &box);
}

void renderMenu(SDL_Renderer* renderer) {
    SDL_Rect box = { 40, 40, mapWidth * TILE_SIZE - 80, mapHeight * TILE_SIZE - 80 };
    SDL_SetRenderDrawColor(renderer, 20, 20, 80, 255);
    SDL_RenderFillRect(renderer, &box);

    const int x = 60;
    int y = 60;
    const int lineH = 20; // 8px * scale(2) + 4px gap

    SDL_SetRenderDrawColor(renderer, 220, 220, 255, 255);
    drawText(renderer, x, y, "STATS");
    y += lineH + 4;

    char buf[32];

    SDL_SetRenderDrawColor(renderer, 100, 220, 100, 255);
    snprintf(buf, sizeof(buf), "HP:  %d", player.maxHp);
    drawText(renderer, x, y, buf); y += lineH;

    SDL_SetRenderDrawColor(renderer, 220, 100, 100, 255);
    snprintf(buf, sizeof(buf), "ATK: %d", player.attack);
    drawText(renderer, x, y, buf); y += lineH;

    SDL_SetRenderDrawColor(renderer, 100, 160, 220, 255);
    snprintf(buf, sizeof(buf), "DEF: %d", player.defense);
    drawText(renderer, x, y, buf); y += lineH;

    SDL_SetRenderDrawColor(renderer, 200, 200, 100, 255);
    snprintf(buf, sizeof(buf), "WPN: %d", player.weaponId);
    drawText(renderer, x, y, buf); y += lineH;
    snprintf(buf, sizeof(buf), "ARM: %d", player.armorId);
    drawText(renderer, x, y, buf); y += lineH + 4;

    if (player.abilityCount > 0) {
        SDL_SetRenderDrawColor(renderer, 180, 120, 220, 255);
        drawText(renderer, x, y, "ABILITIES:"); y += lineH;
        for (int i = 0; i < player.abilityCount && i < 8; i++) {
            snprintf(buf, sizeof(buf), "  %d", player.abilities[i]);
            drawText(renderer, x, y, buf); y += lineH;
        }
    }
}

void renderDialog(SDL_Renderer* renderer) {
    // Placeholder — dialog system coming soon
    SDL_Rect box = { 40, 40, mapWidth * TILE_SIZE - 80, mapHeight * TILE_SIZE - 80 };
    SDL_SetRenderDrawColor(renderer, 150, 120, 0, 255);
    SDL_RenderFillRect(renderer, &box);
}

void renderDungeon(SDL_Renderer* renderer) {
    // Placeholder — dungeon system coming soon
    SDL_Rect box = { 40, 40, mapWidth * TILE_SIZE - 80, mapHeight * TILE_SIZE - 80 };
    SDL_SetRenderDrawColor(renderer, 80, 0, 80, 255);
    SDL_RenderFillRect(renderer, &box);
}

// --- Main ---

int main() {
    if (!pakOpen("data.pak")) return 1;
    if (!loadMap(pakRead("assets/map1.bin"))) return 1;
    if (!loadPlayer(pakRead("assets/player.dat"))) return 1;
    pakClose();

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "smolhero",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        mapWidth * TILE_SIZE, mapHeight * TILE_SIZE,
        0
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            switch (state) {
                case STATE_WORLD:  handleWorldInput(event);  break;
                case STATE_COMBAT: handleCombatInput(event); break;
                case STATE_MENU:   handleMenuInput(event);   break;
                case STATE_DIALOG:  handleDialogInput(event);  break;
                case STATE_DUNGEON: handleDungeonInput(event); break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        switch (state) {
            case STATE_WORLD:  renderWorld(renderer);  break;
            case STATE_COMBAT: renderCombat(renderer); break;
            case STATE_MENU:   renderMenu(renderer);   break;
            case STATE_DIALOG:  renderDialog(renderer);  break;
            case STATE_DUNGEON: renderDungeon(renderer); break;
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
