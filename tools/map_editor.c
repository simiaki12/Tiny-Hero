#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_W 64
#define MAX_H 64

static uint8_t mapGfx[MAX_W * MAX_H];
static uint8_t mapLoc[MAX_W * MAX_H];
static int mapW = 10, mapH = 8;
static int curX = 0, curY = 0;
static const char *outfile = "assets/map1.bin";

static void loadMap(void) {
    FILE *f = fopen(outfile, "rb");
    if (!f) return;
    uint8_t w, h;
    (void)fread(&w, 1, 1, f);
    (void)fread(&h, 1, 1, f);
    mapW = w; mapH = h;
    int n = mapW * mapH;
    (void)fread(mapGfx, 1, n, f);
    (void)fread(mapLoc, 1, n, f);
    fclose(f);
}

static void saveMap(void) {
    FILE *f = fopen(outfile, "wb");
    if (!f) return;
    uint8_t w = (uint8_t)mapW, h = (uint8_t)mapH;
    fwrite(&w, 1, 1, f);
    fwrite(&h, 1, 1, f);
    int n = mapW * mapH;
    fwrite(mapGfx, 1, n, f);
    fwrite(mapLoc, 1, n, f);
    fclose(f);
}

static void drawMap(void) {
    for (int y = 0; y < mapH; y++) {
        for (int x = 0; x < mapW; x++) {
            int idx = y * mapW + x;
            char ch;
            int attr;
            if (mapGfx[idx] == 1) {
                ch = '#'; attr = COLOR_PAIR(1);
            } else {
                switch (mapLoc[idx]) {
                    case 1: ch = 'E'; attr = COLOR_PAIR(2); break;
                    case 2: ch = 'T'; attr = COLOR_PAIR(3); break;
                    case 3: ch = 'D'; attr = COLOR_PAIR(4); break;
                    default: ch = '.'; attr = COLOR_PAIR(5); break;
                }
            }
            if (x == curX && y == curY) attr |= A_REVERSE;
            attron(attr);
            mvaddch(y + 2, x * 2, ch);
            attroff(attr);
        }
    }
}

int main(void) {
    loadMap();

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();

    init_pair(1, COLOR_WHITE,   COLOR_BLACK);  /* wall    */
    init_pair(2, COLOR_RED,     COLOR_BLACK);  /* enemy   */
    init_pair(3, COLOR_YELLOW,  COLOR_BLACK);  /* town    */
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK);  /* dungeon */
    init_pair(5, COLOR_GREEN,   COLOR_BLACK);  /* floor   */

    int running = 1;
    int dirty   = 0;

    while (running) {
        clear();
        mvprintw(0, 0, "MAP EDITOR  %dx%d  [%s]", mapW, mapH, dirty ? "unsaved" : "saved");
        mvprintw(1, 0, "Arrows=move  W=#wall  F=.floor  E=enemy  T=town  D=dungeon  C=clear-loc  S=save  Q=quit");
        drawMap();
        mvprintw(mapH + 3, 0, "cursor (%d,%d)  gfx=%d loc=%d",
                 curX, curY, mapGfx[curY * mapW + curX], mapLoc[curY * mapW + curX]);
        refresh();

        int ch = getch();
        int idx = curY * mapW + curX;
        switch (ch) {
            case KEY_UP:    if (curY > 0)      curY--; break;
            case KEY_DOWN:  if (curY < mapH-1) curY++; break;
            case KEY_LEFT:  if (curX > 0)      curX--; break;
            case KEY_RIGHT: if (curX < mapW-1) curX++; break;
            case ' ':
                mapGfx[idx] ^= 1;
                if (mapGfx[idx]) mapLoc[idx] = 0;
                dirty = 1;
                break;
            case 'w': case 'W':
                mapGfx[idx] = 1; mapLoc[idx] = 0;
                dirty = 1;
                break;
            case 'f': case 'F':
                mapGfx[idx] = 0;
                dirty = 1;
                break;
            case 'e': case 'E':
                mapGfx[idx] = 0; mapLoc[idx] = 1;
                dirty = 1;
                break;
            case 't': case 'T':
                mapGfx[idx] = 0; mapLoc[idx] = 2;
                dirty = 1;
                break;
            case 'd': case 'D':
                mapGfx[idx] = 0; mapLoc[idx] = 3;
                dirty = 1;
                break;
            case 'c': case 'C':
                mapLoc[idx] = 0;
                dirty = 1;
                break;
            case 's': case 'S':
                saveMap();
                dirty = 0;
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
        if (ch == 'y' || ch == 'Y') saveMap();
    }

    return 0;
}
