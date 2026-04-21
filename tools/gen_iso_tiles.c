/* tools/gen_iso_tiles.c — generates assets/tiles/*.bin
 * Build + run: make gen_iso_tiles
 *
 * Binary format matches img_conv.c:
 *   [u8 w][u8 h][u8 n_colors][n_colors * 3 bytes RGB palette]
 *   [packed pixel indices MSB-first; index == n_colors → transparent]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── canvas ──────────────────────────────────────────── */
#define CW 64
#define MAXH 64
static uint8_t G[MAXH * CW * 4]; /* RGBA, row-major */
static int cH;

static void canvas(int h) {
    cH = h;
    memset(G, 0, sizeof(G));
}
static void px(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= CW || y < 0 || y >= cH) return;
    int i = (y * CW + x) * 4;
    G[i]=r; G[i+1]=g; G[i+2]=b; G[i+3]=255;
}

/* ── deterministic noise ─────────────────────────────── */
static uint32_t nh(int x, int y) {
    uint32_t h = (uint32_t)(x * 374761393u + y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static int ni(int x, int y, int n) { return (int)(nh(x, y) % (uint32_t)n); }

/* ── shape tests ─────────────────────────────────────── */
/* Top diamond: matches drawIsoTile span formula exactly.
   Row r=y+1, span=(r<=16)?r*4:(32-r)*4, centre at x=32. */
static int in_top(int x, int y) {
    int r = y + 1;
    if (r < 1 || r > 31) return 0;
    int span = (r <= 16) ? r * 4 : (32 - r) * 4;
    return x >= 32 - span/2 && x < 32 + span/2;
}
static int in_left_wall(int x, int y, int wh) {
    if (x < 0 || x >= 32) return 0;
    int ty = 16 + x / 2;
    return y >= ty && y < ty + wh;
}
static int in_right_wall(int x, int y, int wh) {
    if (x < 32 || x >= CW) return 0;
    int col = x - 32;
    int ty = 32 - col / 2;
    return y >= ty && y < ty + wh;
}

/* ── painters ────────────────────────────────────────── */
typedef void(*CF)(int,int,uint8_t*,uint8_t*,uint8_t*);

static void paint_top(CF f) {
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < CW; x++) {
            if (!in_top(x, y)) continue;
            uint8_t r,g,b; f(x,y,&r,&g,&b); px(x,y,r,g,b);
        }
}
static void paint_left(int wh, CF f) {
    for (int x = 0; x < 32; x++)
        for (int y = 16+x/2; y < 16+x/2+wh && y < cH; y++) {
            uint8_t r,g,b; f(x,y,&r,&g,&b); px(x,y,r,g,b);
        }
}
static void paint_right(int wh, CF f) {
    for (int x = 32; x < CW; x++) {
        int ty = 32 - (x-32)/2;
        for (int y = ty; y < ty+wh && y < cH; y++) {
            uint8_t r,g,b; f(x,y,&r,&g,&b); px(x,y,r,g,b);
        }
    }
}

/* ── binary writer ───────────────────────────────────── */
static int write_bin(const char *path) {
    uint8_t pr[127],pg[127],pb[127]; int nc=0;
    int total = CW * cH;
    for (int i = 0; i < total; i++) {
        if (G[i*4+3] < 25) continue;
        uint8_t r=G[i*4], g=G[i*4+1], b=G[i*4+2];
        int found=0;
        for (int c=0; c<nc; c++)
            if (pr[c]==r && pg[c]==g && pb[c]==b) { found=1; break; }
        if (!found) {
            if (nc >= 127) { fprintf(stderr,"%s: too many colors\n",path); return 0; }
            pr[nc]=r; pg[nc]=g; pb[nc]=b; nc++;
        }
    }
    uint8_t *idx = malloc((size_t)total);
    for (int y=0; y<cH; y++) for (int x=0; x<CW; x++) {
        int i = y*CW+x;
        if (G[i*4+3] < 25) { idx[i]=(uint8_t)nc; continue; }
        uint8_t r=G[i*4], g=G[i*4+1], b=G[i*4+2];
        for (int c=0; c<nc; c++)
            if (pr[c]==r && pg[c]==g && pb[c]==b) { idx[i]=(uint8_t)c; break; }
    }
    int bpp=1; while ((1<<bpp) < nc+1) bpp++;
    int mask=(1<<bpp)-1;
    int db=(total*bpp+7)/8;
    uint8_t *pk=calloc(1,(size_t)db);
    int bp=0;
    for (int i=0; i<total; i++) {
        int val=idx[i]&mask;
        for (int b=bpp-1; b>=0; b--) {
            if ((val>>b)&1) pk[bp/8]|=(uint8_t)(1<<(7-(bp%8)));
            bp++;
        }
    }
    FILE *f=fopen(path,"wb");
    if (!f) { fprintf(stderr,"cannot open %s\n",path); free(idx); free(pk); return 0; }
    uint8_t hdr[3]={(uint8_t)CW,(uint8_t)cH,(uint8_t)nc};
    fwrite(hdr,1,3,f);
    for (int c=0; c<nc; c++) { uint8_t rgb[3]={pr[c],pg[c],pb[c]}; fwrite(rgb,1,3,f); }
    fwrite(pk,1,(size_t)db,f);
    fclose(f);
    printf("  %-35s %dx%d  %dc  %dbpp  %dB\n",
           path, CW, cH, nc, bpp, 3+nc*3+db);
    free(idx); free(pk);
    return 1;
}

/* ══════════════════════════════════════════════════════
 *  COLOUR FUNCTIONS — one per surface, per tile type
 * ══════════════════════════════════════════════════════ */

/* ── GRASS variants ────────────────────────────────── */
static void cf_grass(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    /* scattered blades via 2-D hash; blades lean toward upper-left */
    int blade = (ni(x,y,7)==0) && (ni(x+1,y-1,5)==0);
    int v = ni(x,y,10);
    if (blade)          { *r=28; *g=72; *b=16; }
    else if (v < 2)     { *r=35; *g=88; *b=20; }
    else if (v < 7)     { *r=52; *g=113; *b=30; }
    else if (v < 9)     { *r=68; *g=138; *b=42; }
    else                { *r=80; *g=155; *b=50; }
}
static void cf_grass_enemy(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    cf_grass(x,y,r,g,b);
    *r = (uint8_t)(*r + 65 > 255 ? 255 : *r + 65);
    *g = (uint8_t)(*g > 50 ? *g - 50 : 0);
}
static void cf_grass_town(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    cf_grass(x,y,r,g,b);
    *r = (uint8_t)(*r + 55 > 255 ? 255 : *r + 55);
    *g = (uint8_t)(*g + 15 > 255 ? 255 : *g + 15);
    *b = (uint8_t)(*b > 15 ? *b - 15 : 0);
}
static void cf_grass_dungeon(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    cf_grass(x,y,r,g,b);
    *r = (uint8_t)(*r + 35 > 255 ? 255 : *r + 35);
    *g = (uint8_t)(*g > 45 ? *g - 45 : 0);
    *b = (uint8_t)(*b + 55 > 255 ? 255 : *b + 55);
}
static void cf_grass_portal(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    cf_grass(x,y,r,g,b);
    *g = (uint8_t)(*g + 18 > 255 ? 255 : *g + 18);
    *b = (uint8_t)(*b + 65 > 255 ? 255 : *b + 65);
}

/* ── WATER ────────────────────────────────────────── */
static void cf_water(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int wave = (x/2 + y) % 8;
    /* sparkle: small bright specks */
    int spark = (ni(x,y,22)==0);
    if (spark)          { *r=170; *g=220; *b=255; }
    else if (wave < 2)  { *r=55;  *g=128; *b=210; }
    else if (wave < 5)  { *r=32;  *g=88;  *b=170; }
    else                { *r=18;  *g=55;  *b=130; }
}

/* ── ROAD ─────────────────────────────────────────── */
static void cf_road(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int track = (x + y * 2) % 18;
    if (track < 2)          { *r=62;  *g=52;  *b=34; }
    else if (track < 7)     { *r=105; *g=90;  *b=62; }
    else if (track == 8 || track == 9) { *r=128; *g=112; *b=80; }
    else if (track < 16)    { *r=105; *g=90;  *b=62; }
    else                    { *r=62;  *g=52;  *b=34; }
}

/* ── BRIDGE ──────────────────────────────────────── */
static void cf_bridge(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int plank = ((x - y + 96) / 5) % 3;
    /* plank-end grain at edges */
    int grain = (ni(x,y+x/5,8) < 2);
    if (plank == 2)         { *r=38;  *g=25;  *b=12; } /* gap */
    else if (grain)         { *r=82;  *g=55;  *b=28; }
    else if (plank == 0)    { *r=112; *g=76;  *b=40; }
    else                    { *r=96;  *g=64;  *b=32; }
}

/* ── BUILDING FLOOR ──────────────────────────────── */
static void cf_bldg(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (x % 10 == 0 || y % 10 == 0) { *r=98;  *g=88;  *b=72; return; }
    int tx=x/10, ty=y/10;
    if ((tx+ty)%2==0)       { *r=192; *g=178; *b=150; }
    else                    { *r=162; *g=148; *b=122; }
}

/* ── CAVE FLOOR ──────────────────────────────────── */
static void cf_cave_floor(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    /* sparse cracks */
    int crack = (ni(x,y,30)==0 && ni(x+1,y,4)==0);
    int v = ni(x,y,6);
    if (crack)          { *r=22;  *g=20;  *b=26; }
    else if (v < 2)     { *r=38;  *g=36;  *b=43; }
    else if (v < 5)     { *r=50;  *g=48;  *b=56; }
    else                { *r=62;  *g=60;  *b=70; }
}

/* ── HILLS top ───────────────────────────────────── */
static void cf_hills_top(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int v = ni(x,y,8);
    if (v < 2)          { *r=42;  *g=100; *b=25; }
    else if (v < 6)     { *r=60;  *g=125; *b=35; }
    else if (v < 7)     { *r=75;  *g=148; *b=48; }
    else                { *r=85;  *g=162; *b=55; }
}
static void cf_hills_left(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int lv = ni(x, y, 4);
    if (lv < 2)         { *r=78;  *g=52;  *b=26; }
    else                { *r=92;  *g=65;  *b=33; }
}
static void cf_hills_right(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int lv = ni(x, y, 4);
    if (lv < 2)         { *r=58;  *g=38;  *b=18; }
    else                { *r=68;  *g=47;  *b=22; }
}

/* ── STONE WALL helpers ──────────────────────────── */
/* brick_row / brick_col give the mortar-grid position */
static int brick_offset(int local_y) { return (local_y / 8) % 2 ? 6 : 0; }

static void cf_stone_top(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int v = ni(x,y,6);
    if (v < 2)          { *r=108; *g=105; *b=113; }
    else if (v < 5)     { *r=128; *g=125; *b=133; }
    else                { *r=148; *g=145; *b=154; }
}
static void cf_stone_left(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int ly = y - (16 + x/2);
    if (ly < 0) ly = 0;
    /* mortar rows and vertical joints */
    if (ly % 8 == 0)                            { *r=148; *g=145; *b=152; return; }
    if ((x + brick_offset(ly)) % 12 == 0)       { *r=148; *g=145; *b=152; return; }
    int v = ni(x + ly*3, ly, 5);
    if (v < 2)          { *r=68;  *g=66;  *b=73; }
    else if (v < 4)     { *r=80;  *g=78;  *b=86; }
    else                { *r=90;  *g=88;  *b=97; }
}
static void cf_stone_right(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int col = x - 32;
    int ly = y - (32 - col/2);
    if (ly < 0) ly = 0;
    if (ly % 8 == 0)                            { *r=112; *g=110; *b=116; return; }
    if ((col + brick_offset(ly)) % 12 == 0)     { *r=112; *g=110; *b=116; return; }
    int v = ni(x + ly*3, ly, 5);
    if (v < 2)          { *r=48;  *g=46;  *b=52; }
    else if (v < 4)     { *r=58;  *g=56;  *b=63; }
    else                { *r=66;  *g=64;  *b=72; }
}

/* ── CAVE WALL helpers ───────────────────────────── */
static void cf_cave_top(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int v = ni(x,y,5);
    if (v < 2)          { *r=22;  *g=21;  *b=27; }
    else if (v < 4)     { *r=35;  *g=34;  *b=42; }
    else                { *r=46;  *g=45;  *b=54; }
}
static void cf_cave_left(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int ly = y - (16 + x/2);
    if (ly < 0) ly = 0;
    if (ly % 11 == 0 || ly % 11 == 10)         { *r=52;  *g=50;  *b=60; return; }
    int v = ni(x, ly, 4);
    if (v < 2)          { *r=18;  *g=17;  *b=22; }
    else                { *r=28;  *g=27;  *b=34; }
}
static void cf_cave_right(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int col = x - 32;
    int ly = y - (32 - col/2);
    if (ly < 0) ly = 0;
    if (ly % 11 == 0 || ly % 11 == 10)         { *r=38;  *g=37;  *b=44; return; }
    int v = ni(x, ly, 4);
    if (v < 2)          { *r=12;  *g=11;  *b=15; }
    else                { *r=20;  *g=19;  *b=24; }
}

/* ── MOUNTAINS ───────────────────────────────────── */
static void cf_mtn_top(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    /* snow cap in the upper-centre of the diamond */
    int dist = abs(x - 32) + y * 2;
    if (dist < 12)      { *r=238; *g=240; *b=245; return; } /* snow */
    int v = ni(x,y,6);
    if (v < 2)          { *r=112; *g=108; *b=118; }
    else if (v < 5)     { *r=132; *g=128; *b=138; }
    else                { *r=152; *g=148; *b=158; }
}
static void cf_mtn_left(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int ly = y - (16 + x/2);
    if (ly < 0) ly = 0;
    /* rocky crevices */
    if ((ly + x/3) % 9 == 0)                   { *r=60;  *g=58;  *b=65; return; }
    int v = ni(x + ly, y, 5);
    if (v < 2)          { *r=78;  *g=75;  *b=83; }
    else if (v < 4)     { *r=90;  *g=87;  *b=96; }
    else                { *r=100; *g=97;  *b=107; }
}
static void cf_mtn_right(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int col = x - 32;
    int ly = y - (32 - col/2);
    if (ly < 0) ly = 0;
    if ((ly + col/3) % 9 == 0)                 { *r=42;  *g=40;  *b=47; return; }
    int v = ni(x + ly, y, 5);
    if (v < 2)          { *r=55;  *g=53;  *b=60; }
    else if (v < 4)     { *r=65;  *g=63;  *b=71; }
    else                { *r=75;  *g=73;  *b=82; }
}

/* ── TREE ────────────────────────────────────────── */
static void cf_tree_top(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    /* canopy clusters: 8-pixel blob pattern */
    int bx = (x/8) * 7 + (y/8) * 3;
    int blob = ni(bx, y/8, 4);
    if (blob == 0)      { *r=15;  *g=62;  *b=12; }
    else if (blob < 3)  { *r=22;  *g=82;  *b=18; }
    else                { *r=30;  *g=100; *b=24; }
    /* random bright leaf tips */
    if (ni(x,y,18)==0)  { *r=42;  *g=118; *b=32; }
}
static void cf_tree_left(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int ly = y - (16 + x/2);
    if (ly < 0) ly = 0;
    /* trunk lower half, foliage upper */
    if (ly > 16)        { *r=62;  *g=40;  *b=18; }
    else                { *r=20;  *g=68;  *b=15; }
    if (ly > 16 && ni(x,ly,6)==0) { *r=45; *g=28; *b=10; } /* bark mark */
}
static void cf_tree_right(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    int col = x - 32;
    int ly = y - (32 - col/2);
    if (ly < 0) ly = 0;
    if (ly > 16)        { *r=45;  *g=28;  *b=12; }
    else                { *r=14;  *g=50;  *b=10; }
    if (ly > 16 && ni(x,ly,6)==0) { *r=30; *g=18; *b=6; }
}

/* ══════════════════════════════════════════════════════
 *  PLAYER SPRITE — 16×24, two walk frames
 * ══════════════════════════════════════════════════════ */
#define SPR_W 16
#define SPR_H 24

/* Index 0 = transparent; 1-9 = palette entries below */
static const uint8_t SPR_PAL[10][3] = {
    {0,0,0},          /* 0: unused (transparent) */
    {150,155,170},    /* 1: silver helm highlight */
    {80,83,95},       /* 2: helm shadow */
    {205,158,108},    /* 3: skin */
    {50,65,105},      /* 4: blue armor */
    {30,40,68},       /* 5: armor shadow */
    {195,198,215},    /* 6: sword silver */
    {60,38,20},       /* 7: dark boot/belt */
    {125,25,25},      /* 8: red cape */
    {195,158,48},     /* 9: gold trim */
};

static int write_player_bin(const char *path, const uint8_t pix[SPR_H][SPR_W]) {
    uint8_t pr[32],pg[32],pb[32]; int nc=0;
    for (int y=0;y<SPR_H;y++) for (int x=0;x<SPR_W;x++) {
        int ci=pix[y][x]; if (!ci) continue;
        uint8_t r=SPR_PAL[ci][0],g=SPR_PAL[ci][1],b=SPR_PAL[ci][2];
        int found=0;
        for (int c=0;c<nc;c++) if (pr[c]==r&&pg[c]==g&&pb[c]==b){found=1;break;}
        if (!found){pr[nc]=r;pg[nc]=g;pb[nc]=b;nc++;}
    }
    int total=SPR_W*SPR_H;
    uint8_t *ia=malloc((size_t)total);
    for (int y=0;y<SPR_H;y++) for (int x=0;x<SPR_W;x++) {
        int pi=y*SPR_W+x, ci=pix[y][x];
        if (!ci){ia[pi]=(uint8_t)nc;continue;}
        uint8_t r=SPR_PAL[ci][0],g=SPR_PAL[ci][1],b=SPR_PAL[ci][2];
        for (int c=0;c<nc;c++) if (pr[c]==r&&pg[c]==g&&pb[c]==b){ia[pi]=(uint8_t)c;break;}
    }
    int bpp=1; while((1<<bpp)<nc+1) bpp++;
    int db=(total*bpp+7)/8;
    uint8_t *pk=calloc(1,(size_t)db);
    int bp=0;
    for (int i=0;i<total;i++){
        int val=ia[i]&((1<<bpp)-1);
        for (int b=bpp-1;b>=0;b--){
            if((val>>b)&1) pk[bp/8]|=(uint8_t)(1<<(7-(bp%8)));
            bp++;
        }
    }
    FILE *f=fopen(path,"wb");
    if (!f){fprintf(stderr,"cannot open %s\n",path);free(ia);free(pk);return 0;}
    uint8_t hdr[3]={(uint8_t)SPR_W,(uint8_t)SPR_H,(uint8_t)nc};
    fwrite(hdr,1,3,f);
    for (int c=0;c<nc;c++){uint8_t rgb[3]={pr[c],pg[c],pb[c]};fwrite(rgb,1,3,f);}
    fwrite(pk,1,(size_t)db,f);
    fclose(f);
    printf("  %-35s %dx%d  %dc  %dbpp  %dB\n",path,SPR_W,SPR_H,nc,bpp,3+nc*3+db);
    free(ia);free(pk);
    return 1;
}

static void gen_player(void) {
    system("mkdir -p assets/sprites");

    /* palette: 0=transparent, 1=silver helm, 2=helm shadow, 3=skin,
       4=armor, 5=armor shadow, 6=sword, 7=boot/belt, 8=cape, 9=gold */
    static const uint8_t A[SPR_H][SPR_W] = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, /*  0 */
        {0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0}, /*  1 helmet */
        {0,0,0,0,1,2,1,1,2,1,0,0,0,0,0,0}, /*  2 */
        {0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0}, /*  3 */
        {0,0,0,0,2,3,3,3,3,2,0,0,0,0,0,0}, /*  4 visor/face */
        {0,0,0,0,0,3,3,3,0,0,0,0,0,0,0,0}, /*  5 neck */
        {0,0,0,4,4,4,4,4,4,4,0,0,0,0,0,0}, /*  6 gorget */
        {0,0,4,4,5,4,4,4,9,4,0,6,0,0,0,0}, /*  7 shoulder+sword */
        {0,0,4,5,4,4,4,9,0,0,0,6,0,0,0,0}, /*  8 */
        {0,8,4,5,4,4,4,9,0,0,6,6,0,0,0,0}, /*  9 cape */
        {0,8,8,5,4,4,4,0,0,0,0,0,0,0,0,0}, /* 10 */
        {0,8,8,0,5,4,4,5,0,0,0,0,0,0,0,0}, /* 11 waist */
        {0,8,0,0,7,4,4,7,0,0,0,0,0,0,0,0}, /* 12 hips */
        {0,0,0,0,7,7,7,7,0,0,0,0,0,0,0,0}, /* 13 belt */
        {0,0,0,7,7,0,0,7,7,0,0,0,0,0,0,0}, /* 14 thighs */
        {0,0,0,7,7,0,0,7,7,0,0,0,0,0,0,0}, /* 15 */
        {0,0,0,7,7,0,0,7,0,0,0,0,0,0,0,0}, /* 16 left forward */
        {0,0,0,7,7,0,0,0,0,0,0,0,0,0,0,0}, /* 17 */
        {0,0,0,7,7,0,0,0,0,0,0,0,0,0,0,0}, /* 18 */
        {0,0,7,7,0,0,0,0,0,0,0,0,0,0,0,0}, /* 19 left foot */
        {0,0,7,7,0,0,0,0,0,0,0,0,0,0,0,0}, /* 20 */
        {0,0,0,0,0,0,0,7,7,0,0,0,0,0,0,0}, /* 21 right back */
        {0,0,0,0,0,0,0,7,7,0,0,0,0,0,0,0}, /* 22 */
        {0,0,0,0,0,0,7,7,7,0,0,0,0,0,0,0}, /* 23 right foot back */
    };

    static const uint8_t B[SPR_H][SPR_W] = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, /*  0 */
        {0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0}, /*  1 */
        {0,0,0,0,1,2,1,1,2,1,0,0,0,0,0,0}, /*  2 */
        {0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0}, /*  3 */
        {0,0,0,0,2,3,3,3,3,2,0,0,0,0,0,0}, /*  4 */
        {0,0,0,0,0,3,3,3,0,0,0,0,0,0,0,0}, /*  5 */
        {0,0,0,4,4,4,4,4,4,4,0,0,0,0,0,0}, /*  6 */
        {0,0,4,4,5,4,4,4,9,4,0,6,0,0,0,0}, /*  7 */
        {0,0,4,5,4,4,4,9,0,0,0,6,0,0,0,0}, /*  8 */
        {0,8,4,5,4,4,4,9,0,0,6,6,0,0,0,0}, /*  9 */
        {0,8,8,5,4,4,4,0,0,0,0,0,0,0,0,0}, /* 10 */
        {0,8,8,0,5,4,4,5,0,0,0,0,0,0,0,0}, /* 11 */
        {0,8,0,0,7,4,4,7,0,0,0,0,0,0,0,0}, /* 12 */
        {0,0,0,0,7,7,7,7,0,0,0,0,0,0,0,0}, /* 13 */
        {0,0,0,7,7,0,0,7,7,0,0,0,0,0,0,0}, /* 14 thighs */
        {0,0,0,7,7,0,0,7,7,0,0,0,0,0,0,0}, /* 15 */
        {0,0,0,0,7,0,0,7,7,0,0,0,0,0,0,0}, /* 16 right forward */
        {0,0,0,0,0,0,0,7,7,0,0,0,0,0,0,0}, /* 17 */
        {0,0,0,0,0,0,0,7,7,0,0,0,0,0,0,0}, /* 18 */
        {0,0,0,0,0,0,0,7,7,0,0,0,0,0,0,0}, /* 19 right foot */
        {0,0,0,0,0,0,7,7,7,0,0,0,0,0,0,0}, /* 20 */
        {0,0,0,7,7,0,0,0,0,0,0,0,0,0,0,0}, /* 21 left back */
        {0,0,0,7,7,0,0,0,0,0,0,0,0,0,0,0}, /* 22 */
        {0,0,7,7,7,0,0,0,0,0,0,0,0,0,0,0}, /* 23 left foot back */
    };

    write_player_bin("assets/sprites/player_iso_a.bin", A);
    write_player_bin("assets/sprites/player_iso_b.bin", B);
}

/* ══════════════════════════════════════════════════════ */
int main(void) {
    system("mkdir -p assets/tiles");
    printf("Generating iso tiles...\n");

    /* ── flat tiles (64×32) ──────────────────────────── */
    canvas(32);
    paint_top(cf_grass);       write_bin("assets/tiles/grass.bin");
    canvas(32);
    paint_top(cf_grass_enemy); write_bin("assets/tiles/grass_enemy.bin");
    canvas(32);
    paint_top(cf_grass_town);  write_bin("assets/tiles/grass_town.bin");
    canvas(32);
    paint_top(cf_grass_dungeon);write_bin("assets/tiles/grass_dungeon.bin");
    canvas(32);
    paint_top(cf_grass_portal);write_bin("assets/tiles/grass_portal.bin");
    canvas(32);
    paint_top(cf_water);       write_bin("assets/tiles/river.bin");
    canvas(32);
    paint_top(cf_road);        write_bin("assets/tiles/road.bin");
    canvas(32);
    paint_top(cf_bridge);      write_bin("assets/tiles/bridge.bin");
    canvas(32);
    paint_top(cf_bldg);        write_bin("assets/tiles/bldg_floor.bin");
    canvas(32);
    paint_top(cf_cave_floor);  write_bin("assets/tiles/cave_floor.bin");

    /* ── half-block tiles (64×48, wall_h=16) ────────── */
    canvas(48);
    paint_top(cf_hills_top);
    paint_left(16, cf_hills_left);
    paint_right(16, cf_hills_right);
    write_bin("assets/tiles/hills.bin");

    /* ── full-block tiles (64×64, wall_h=32) ─────────── */
    canvas(64);
    paint_top(cf_stone_top);
    paint_left(32, cf_stone_left);
    paint_right(32, cf_stone_right);
    write_bin("assets/tiles/wall.bin");

    canvas(64);
    paint_top(cf_cave_top);
    paint_left(32, cf_cave_left);
    paint_right(32, cf_cave_right);
    write_bin("assets/tiles/cave_wall.bin");

    canvas(64);
    paint_top(cf_mtn_top);
    paint_left(32, cf_mtn_left);
    paint_right(32, cf_mtn_right);
    write_bin("assets/tiles/mountains.bin");

    canvas(64);
    paint_top(cf_tree_top);
    paint_left(32, cf_tree_left);
    paint_right(32, cf_tree_right);
    write_bin("assets/tiles/tree.bin");

    printf("Generating player sprites...\n");
    gen_player();

    printf("Done.\n");
    return 0;
}
