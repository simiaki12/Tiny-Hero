#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define MAX_W        255
#define MAX_H        255
#define MAX_MAPFILES 32

static uint8_t mapGfx[MAX_W * MAX_H];
static uint8_t mapLoc[MAX_W * MAX_H];
static int mapW = 10, mapH = 8;
static int curX = 0, curY = 0;
static int spawnX = 2, spawnY = 2;

/* transition target preserved verbatim on load/save; not editable in this tool */
static char transitionTarget[64] = {0};

#define MAX_PORTALS      16
#define LOC_PORTAL_BASE  0xE0
static char    portalTargets[MAX_PORTALS][64] = {{0}};
static uint8_t portalSpawnX[MAX_PORTALS]      = {0};
static uint8_t portalSpawnY[MAX_PORTALS]      = {0};

static char outfile[64] = "assets/map1.bin";

/* --- Map file discovery ---------------------------------------- */
static char mapFiles[MAX_MAPFILES][64];
static int  mapFileCount = 0;

static void scanMaps(void) {
    mapFileCount = 0;
    DIR *d = opendir("assets");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && mapFileCount < MAX_MAPFILES) {
        int len = (int)strlen(e->d_name);
        if (len > 4 && len < 56 && strcmp(e->d_name + len - 4, ".bin") == 0) {
            char *p = mapFiles[mapFileCount];
            memcpy(p, "assets/", 7);
            memcpy(p + 7, e->d_name, (size_t)len + 1);
            mapFileCount++;
        }
    }
    closedir(d);
    /* sort alphabetically */
    for (int i = 0; i < mapFileCount - 1; i++)
        for (int j = i + 1; j < mapFileCount; j++)
            if (strcmp(mapFiles[i], mapFiles[j]) > 0) {
                char tmp[64];
                memcpy(tmp, mapFiles[i], 64);
                memcpy(mapFiles[i], mapFiles[j], 64);
                memcpy(mapFiles[j], tmp, 64);
            }
}

static void resetMapState(void) {
    memset(mapGfx, 0, sizeof(mapGfx));
    memset(mapLoc, 0, sizeof(mapLoc));
    mapW = 10; mapH = 8;
    spawnX = 2; spawnY = 2;
    transitionTarget[0] = '\0';
    memset(portalTargets, 0, sizeof(portalTargets));
    memset(portalSpawnX,  0, sizeof(portalSpawnX));
    memset(portalSpawnY,  0, sizeof(portalSpawnY));
    curX = 0; curY = 0;
}

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
        int i = 0, c;
        while (i < 63 && (c = fgetc(f)) != EOF && c != '\0')
            transitionTarget[i++] = (char)c;
        transitionTarget[i] = '\0';

        /* portal table: [count][id, spawnX, spawnY, dest\0] ... */
        memset(portalTargets, 0, sizeof(portalTargets));
        memset(portalSpawnX,  0, sizeof(portalSpawnX));
        memset(portalSpawnY,  0, sizeof(portalSpawnY));
        int pc = fgetc(f);
        if (pc != EOF) {
            for (int p = 0; p < pc; p++) {
                int pid = fgetc(f);
                int sx  = fgetc(f);
                int sy  = fgetc(f);
                if (pid == EOF || sx == EOF || sy == EOF) break;
                i = 0;
                while (i < 63 && (c = fgetc(f)) != EOF && c != '\0')
                    portalTargets[pid][i++] = (char)c;
                if (pid < MAX_PORTALS) {
                    portalTargets[pid][i] = '\0';
                    portalSpawnX[pid] = (uint8_t)sx;
                    portalSpawnY[pid] = (uint8_t)sy;
                }
            }
        }
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

    /* portal table */
    uint8_t pc = 0;
    for (int i = 0; i < MAX_PORTALS; i++) if (portalTargets[i][0]) pc++;
    fwrite(&pc, 1, 1, f);
    for (int i = 0; i < MAX_PORTALS; i++) {
        if (portalTargets[i][0]) {
            uint8_t pid = (uint8_t)i;
            fwrite(&pid,            1, 1, f);
            fwrite(&portalSpawnX[i], 1, 1, f);
            fwrite(&portalSpawnY[i], 1, 1, f);
            fwrite(portalTargets[i], 1, strlen(portalTargets[i]) + 1, f);
        }
    }

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
                uint8_t loc = mapLoc[idx];
                if (loc >= 1 && loc <= 15) {
                    ch = (loc <= 9) ? ('0' + loc) : ('A' + loc - 10);
                    attr = COLOR_PAIR(2);
                } else if (loc == 0xFE) {
                    ch = 'T'; attr = COLOR_PAIR(3);
                } else if (loc == 0xFF) {
                    ch = 'D'; attr = COLOR_PAIR(4);
                } else if (loc >= LOC_PORTAL_BASE && loc < LOC_PORTAL_BASE + MAX_PORTALS) {
                    ch = 'a' + (loc - LOC_PORTAL_BASE);
                    attr = COLOR_PAIR(7);
                } else {
                    ch = '.'; attr = COLOR_PAIR(5);
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
    if (argc >= 2) { strncpy(outfile, argv[1], sizeof(outfile)-1); outfile[sizeof(outfile)-1] = '\0'; }

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
    init_pair(7, COLOR_BLUE,    COLOR_BLACK);  /* portal  */

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
            mvprintw(2, 0, "Arrows=move  W=wall  F=floor  1-9=enemy pool  T=town  D=dungeon  R=portal  C=clear  P=spawn  S=save  O=open  N=new  Q=quit");
        } else {
            mvprintw(1, 0, "Arrows=move  W=wall  F=floor  1-9=enemy pool  T=town  D=dungeon  R=portal  C=clear  P=spawn  S=save  O=open  N=new  Q=quit");
            mvprintw(2, 0, "Legend: #=wall  .=floor  1-9=enemy  T=town  D=dungeon  a-p=portal  @=spawn");
        }

        drawMap();
        {
            uint8_t loc = mapLoc[curY * mapW + curX];
            const char *locDesc;
            char poolBuf[80];
            if      (loc == 0)    locDesc = "floor";
            else if (loc == 0xFE) locDesc = "town";
            else if (loc == 0xFF) locDesc = "dungeon";
            else if (loc <= 15)   { snprintf(poolBuf, sizeof(poolBuf), "enemy pool %d", loc); locDesc = poolBuf; }
            else if (loc >= LOC_PORTAL_BASE && loc < LOC_PORTAL_BASE + MAX_PORTALS) {
                int pid = loc - LOC_PORTAL_BASE;
                snprintf(poolBuf, sizeof(poolBuf), "portal %c -> %s @ (%d,%d)",
                         pid < 10 ? '0' + pid : 'a' + pid - 10,
                         portalTargets[pid][0] ? portalTargets[pid] : "(unset)",
                         portalSpawnX[pid], portalSpawnY[pid]);
                locDesc = poolBuf;
            } else                locDesc = "?";
            mvprintw(mapH + 4, 0, "cursor (%d,%d)  gfx=%d  loc=%s",
                     curX, curY, mapGfx[curY * mapW + curX], locDesc);
        }
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
            case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                mapGfx[idx] = 0; mapLoc[idx] = (uint8_t)(ch - '0');
                dirty = 1;
                break;
            case 't': case 'T':
                mapGfx[idx] = 0; mapLoc[idx] = 0xFE;
                dirty = 1;
                break;
            case 'd': case 'D':
                mapGfx[idx] = 0; mapLoc[idx] = 0xFF;
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
            case 'r': case 'R': {
                /* Place a portal tile and set its destination */
                echo(); curs_set(1);
                mvprintw(mapH + 7, 0, "Portal slot (0-9, a-f): ");
                clrtoeol(); refresh();
                int pid = getch();
                int pidIdx = -1;
                if (pid >= '0' && pid <= '9') pidIdx = pid - '0';
                else if (pid >= 'a' && pid <= 'f') pidIdx = 10 + pid - 'a';
                else if (pid >= 'A' && pid <= 'F') pidIdx = 10 + pid - 'A';
                if (pidIdx >= 0) {
                    mvprintw(mapH + 8, 0, "Destination: assets/");
                    clrtoeol(); refresh();
                    char dest[48] = {0};
                    getnstr(dest, (int)sizeof(dest) - 1);
                    mvprintw(mapH + 9, 0, "Spawn X in dest: ");
                    clrtoeol(); refresh();
                    char numBuf[8] = {0};
                    getnstr(numBuf, (int)sizeof(numBuf) - 1);
                    int sx = atoi(numBuf);
                    mvprintw(mapH + 10, 0, "Spawn Y in dest: ");
                    clrtoeol(); refresh();
                    memset(numBuf, 0, sizeof(numBuf));
                    getnstr(numBuf, (int)sizeof(numBuf) - 1);
                    int sy = atoi(numBuf);
                    if (dest[0]) {
                        snprintf(portalTargets[pidIdx], 64, "assets/%s", dest);
                        portalSpawnX[pidIdx] = (uint8_t)sx;
                        portalSpawnY[pidIdx] = (uint8_t)sy;
                        mapGfx[idx] = 0;
                        mapLoc[idx] = (uint8_t)(LOC_PORTAL_BASE + pidIdx);
                        dirty = 1;
                    }
                }
                noecho(); curs_set(0);
                break;
            }
            case 'o': case 'O': {
                if (dirty) { saveMap(); dirty = 0; fileFound = 1; }
                scanMaps();
                int sel = 0;
                for (int i = 0; i < mapFileCount; i++)
                    if (strcmp(mapFiles[i], outfile) == 0) { sel = i; break; }
                int browsing = 1;
                while (browsing) {
                    clear();
                    mvprintw(0, 0, "OPEN MAP — Arrows=select  Enter=open  Esc=cancel");
                    for (int i = 0; i < mapFileCount; i++) {
                        if (i == sel) attron(A_REVERSE);
                        mvprintw(i + 2, 2, "%s", mapFiles[i]);
                        if (i == sel) attroff(A_REVERSE);
                    }
                    refresh();
                    int c = getch();
                    if (c == KEY_UP   && sel > 0)              sel--;
                    else if (c == KEY_DOWN && sel < mapFileCount-1) sel++;
                    else if (c == '\n' || c == KEY_ENTER) {
                        strncpy(outfile, mapFiles[sel], sizeof(outfile)-1);
                        outfile[sizeof(outfile)-1] = '\0';
                        resetMapState();
                        loadMap();
                        fileFound = 1;
                        browsing = 0;
                    } else if (c == 27) browsing = 0;
                }
                break;
            }
            case 'n': case 'N': {
                if (dirty) { saveMap(); dirty = 0; fileFound = 1; }
                echo(); curs_set(1);
                mvprintw(mapH + 7, 0, "New map name: assets/");
                clrtoeol();
                char nameBuf[32] = {0};
                getnstr(nameBuf, (int)sizeof(nameBuf)-1);
                if (nameBuf[0]) {
                    char dimBuf[8] = {0};
                    mvprintw(mapH + 8, 0, "Width  (1-%d): ", MAX_W);
                    clrtoeol(); refresh();
                    getnstr(dimBuf, (int)sizeof(dimBuf)-1);
                    int newW = atoi(dimBuf);
                    mvprintw(mapH + 9, 0, "Height (1-%d): ", MAX_H);
                    clrtoeol(); refresh();
                    memset(dimBuf, 0, sizeof(dimBuf));
                    getnstr(dimBuf, (int)sizeof(dimBuf)-1);
                    int newH = atoi(dimBuf);
                    noecho(); curs_set(0);
                    snprintf(outfile, sizeof(outfile), "assets/%s.bin", nameBuf);
                    resetMapState();
                    if (newW >= 1 && newW <= MAX_W) mapW = newW;
                    if (newH >= 1 && newH <= MAX_H) mapH = newH;
                    fileFound = 0;
                    dirty = 1;
                } else {
                    noecho(); curs_set(0);
                }
                break;
            }
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
