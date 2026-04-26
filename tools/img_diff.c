#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: img_diff <a.png> <b.png>\n");
        return 1;
    }

    int wa, ha, wb, hb;
    unsigned char *a = stbi_load(argv[1], &wa, &ha, NULL, 1);
    unsigned char *b = stbi_load(argv[2], &wb, &hb, NULL, 1);

    if (!a || !b) { fprintf(stderr, "failed to load image\n"); return 1; }
    if (wa != wb || ha != hb) {
        fprintf(stderr, "size mismatch: %dx%d vs %dx%d\n", wa, ha, wb, hb);
        return 1;
    }

    int diff = 0;
    for (int i = 0; i < wa * ha; i++)
        if ((a[i] >= 128) != (b[i] >= 128)) diff++;

    printf("%d / %d pixels differ (%.2f%%)\n", diff, wa * ha, 100.0 * diff / (wa * ha));

    stbi_image_free(a);
    stbi_image_free(b);
    return 0;
}
