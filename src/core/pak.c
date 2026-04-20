#include "pak.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ENTRIES 256

static PakEntry pakEntries[MAX_ENTRIES];
static int      pakCount = 0;
static FILE*    pakFile  = NULL;

int pakOpen(const char* filename) {
    pakFile = fopen(filename, "rb");
    if (!pakFile) return 0;

    char magic[4];
    fread(magic, 1, 4, pakFile);
    if (strncmp(magic, "PAK0", 4) != 0) { fclose(pakFile); pakFile = NULL; return 0; }

    uint16_t fileCount;
    fread(&fileCount, 2, 1, pakFile);
    if (fileCount > MAX_ENTRIES) fileCount = MAX_ENTRIES;
    pakCount = (int)fileCount;
    fread(pakEntries, sizeof(PakEntry), pakCount, pakFile);
    return 1;
}

static int slashEq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        char ca = (*a == '\\') ? '/' : *a;
        char cb = (*b == '\\') ? '/' : *b;
        if (ca != cb) return 0;
    }
    return *a == *b;
}

const PakEntry* pakFind(const char* name) {
    for (int i = 0; i < pakCount; i++)
        if (slashEq(name, pakEntries[i].name))
            return &pakEntries[i];
    return NULL;
}

PakData pakRead(const char* name) {
    PakData result = {NULL, 0};
    const PakEntry* entry = pakFind(name);
    if (!entry) return result;

    result.data = (uint8_t*)malloc(entry->size);
    if (!result.data) return result;
    result.size = entry->size;
    fseek(pakFile, (long)entry->offset, SEEK_SET);
    fread(result.data, 1, entry->size, pakFile);
    return result;
}

void pakClose(void) {
    if (pakFile) { fclose(pakFile); pakFile = NULL; }
    pakCount = 0;
}
