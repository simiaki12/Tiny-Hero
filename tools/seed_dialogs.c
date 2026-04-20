/* tools/seed_dialogs.c — writes the initial assets/dialog.dat
 * Run once: make seed_dialogs
 * After that, use the dialog editor to modify content. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static void wByte(FILE *f, uint8_t b) { fwrite(&b, 1, 1, f); }
static void wStr(FILE *f, const char *s) {
    uint8_t len = (uint8_t)strlen(s);
    wByte(f, len);
    fwrite(s, 1, len, f);
}
/* next=-1 stored as 0xFF (uint8_t cast of -1) */
#define CLOSE 0xFF
#define NO_SKILL 0xFF
static void wOpt(FILE *f, uint8_t skill, uint8_t level, uint8_t next, const char *text) {
    wByte(f, skill); wByte(f, level); wByte(f, next); wStr(f, text);
}

int main(void) {
    FILE *f = fopen("assets/data/dialog.dat", "wb");
    if (!f) { fprintf(stderr, "Cannot write assets/dialog.dat\n"); return 1; }

    wByte(f, 1); /* tree count */

    /* ---- Tree 0: Innkeeper ---- */
    wStr(f, "Innkeeper");
    wByte(f, 5); /* node count */

    /* Node 0 */
    wStr(f, "Welcome, traveler. What brings you here?");
    wByte(f, 3); /* options */
    wOpt(f, NO_SKILL, 0, 1, "Just passing through.");
    wOpt(f, NO_SKILL, 0, 2, "Tell me about the dungeon.");
    wOpt(f,        3, 2, 3, "I seek an audience with the lord."); /* SKILL_DIPLOMACY=3 */

    /* Node 1 */
    wStr(f, "Safe travels then. Stay out of trouble.");
    wByte(f, 1);
    wOpt(f, NO_SKILL, 0, CLOSE, "Farewell.");

    /* Node 2 */
    wStr(f, "The dungeon to the east claims many lives. Few who enter return.");
    wByte(f, 2);
    wOpt(f, NO_SKILL, 0, CLOSE, "I'll be careful.");
    wOpt(f, NO_SKILL, 0,     4, "What lurks in there?");

    /* Node 3 */
    wStr(f, "A diplomat! The lord's keep is north of the plaza. Ask for the steward.");
    wByte(f, 1);
    wOpt(f, NO_SKILL, 0, CLOSE, "My thanks.");

    /* Node 4 */
    wStr(f, "Dark creatures, and something far worse deep within. You have been warned.");
    wByte(f, 1);
    wOpt(f, NO_SKILL, 0, CLOSE, "I'll face whatever it is.");

    fclose(f);
    printf("Written assets/dialog.dat\n");
    return 0;
}
