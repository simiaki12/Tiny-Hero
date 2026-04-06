/* tools/migrate_map.c — one-time migration of map loc values
 * Old scheme: LOC_TOWN=2, LOC_DUNGEON=3
 * New scheme: LOC_TOWN=0xFE, LOC_DUNGEON=0xFF
 * Usage: migrate_map <mapfile> [mapfile2 ...] */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static int migrateFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return 1; }
    (void)fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (sz < 2) { fprintf(stderr, "%s: too small\n", path); free(buf); return 1; }
    int w = buf[0], h = buf[1];
    int n = w * h;
    int locOffset = 2 + n;
    if (sz < locOffset + n) { fprintf(stderr, "%s: truncated\n", path); free(buf); return 1; }

    int changed = 0;
    for (int i = 0; i < n; i++) {
        if (buf[locOffset + i] == 2) { buf[locOffset + i] = 0xFE; changed++; }
        else if (buf[locOffset + i] == 3) { buf[locOffset + i] = 0xFF; changed++; }
    }

    f = fopen(path, "wb");
    fwrite(buf, 1, (size_t)sz, f);
    fclose(f);
    free(buf);

    printf("%s: %d tile(s) migrated\n", path, changed);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: migrate_map <mapfile> [mapfile2 ...]\n");
        return 1;
    }
    int ret = 0;
    for (int i = 1; i < argc; i++)
        ret |= migrateFile(argv[i]);
    return ret;
}
