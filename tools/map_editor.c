#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_W 64
#define MAX_H 64

static uint8_t mapGfx[MAX_W * MAX_H];
static uint8_t mapLoc[MAX_W * MAX_H];
static int mapW = 10, mapH = 8;
static int curX = 0, curY = 0;
static int spawnX = 2, spawnY = 2;

/* transition target preserved verbatim on load/save; not editable in this tool */
static char transitionTarget[64] = {0};

static const char *outfile = "assets/map1.bin";

static void loadMap(void) {
    FILE *f = fopen(outfile, "rb");
    if (!f) {
        /* file not found — keep defaults and warn after ncurses init */
        return;
    }

    uint8_t w, h;
    if (fread(&w, 1, 1, f) < 1 || fread(&h, 1, 1, f) < 1) { fclose(f); return; }
    mapW = w; mapH = h;
    int n = mapW * mapH;
    (void)fread(mapGfx, 1, n, f);
    (void)fread(mapLoc, 1, n, f);

    /* Optional extension: spawnX, spawnY, null-terminated transition target */
    uint8_t sx, sy;
    if (fread(&sx, 1, 1, f) == 1 && fread(&sy, 1, 1, f) == 1) {
        spawnX = sx;
        spawnY = sy;
        int i = 0;
        int c;
        while (i < 63 && (c = fgetc(f)) != EOF && c != '\0')
            transitionTarget[i++] = (char)c;
        transitionTarget[i] = '\0';
    }

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

    /* Extension: spawn + transition target */
    uint8_t sx = (uint8_t)spawnX, sy = (uint8_t)spawnY;
    fwrite(&sx, 1, 1, f);
    fwrite(&sy, 1, 1, f);
    fwrite(transitionTarget, 1, strlen(transitionTarget) + 1, f);

    fclose(f);
}

static void drawMap(void) {
    for (int y = 0; y < mapH; y++) {
        for (int x = 0; x < mapW; x++) {
            int idx = y * mapW + x;
            char ch;
            int attr;
            if (x == spawnX && y == spawnY && mapGfx[idx] == 0) {
                ch = '@'; attr = COLOR_PAIR(6);
            } else if (mapGfx[idx] == 1) {
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
            mvaddch(y + 3, x * 2, ch);
            attroff(attr);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc >= 2) outfile = argv[1];

    int fileFound = 1;
    {
        FILE *probe = fopen(outfile, "rb");
        if (!probe) fileFound = 0;
        else fclose(probe);
    }

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
    init_pair(6, COLOR_CYAN,    COLOR_BLACK);  /* spawn   */

    int running = 1;
    int dirty   = 0;

    while (running) {
        clear();
        mvprintw(0, 0, "MAP EDITOR  %dx%d  spawn(%d,%d)  [%s]  %s",
                 mapW, mapH, spawnX, spawnY,
                 dirty ? "unsaved" : "saved",
                 fileFound ? outfile : "NEW FILE");
        if (!fileFound) {
            attron(COLOR_PAIR(2));
            mvprintw(1, 0, "WARNING: '%s' not found — starting blank map", outfile);
            attroff(COLOR_PAIR(2));
            mvprintw(2, 0, "Arrows=move  W=#  F=.floor  E=enemy  T=town  D=dungeon  C=clear  P=spawn  S=save  Q=quit");
        } else {
            mvprintw(1, 0, "Arrows=move  W=#  F=.floor  E=enemy  T=town  D=dungeon  C=clear  P=spawn  S=save  Q=quit");
            mvprintw(2, 0, "Legend: #=wall  .=floor  E=enemy  T=town  D=dungeon  @=spawn");
        }

        drawMap();
        mvprintw(mapH + 4, 0, "cursor (%d,%d)  gfx=%d loc=%d",
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
            case 'p': case 'P':
                if (mapGfx[idx] == 0) {
                    spawnX = curX;
                    spawnY = curY;
                    dirty = 1;
                }
                break;
            case 's': case 'S':
                saveMap();
                dirty = 0;
                fileFound = 1;
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
        int c = getchar();
        if (c == 'y' || c == 'Y') saveMap();
    }

    return 0;
}
