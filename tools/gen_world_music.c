/* gen_world_music.c — generates assets/world_music.mid
   A gentle looping adventure theme in C major.
   Compile: gcc -std=c11 -Os tools/gen_world_music.c -o build/gen_world_music
   Run:     ./build/gen_world_music
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* ---- MIDI helpers ---- */

typedef struct { uint8_t *data; size_t len; size_t cap; } Buf;

static void buf_push(Buf *b, uint8_t byte) {
    if (b->len == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->data = realloc(b->data, b->cap);
    }
    b->data[b->len++] = byte;
}

static void buf_varlen(Buf *b, uint32_t n) {
    uint8_t tmp[4]; int i = 0;
    do { tmp[i++] = n & 0x7F; n >>= 7; } while (n);
    for (int j = i - 1; j >= 0; j--)
        buf_push(b, tmp[j] | (j ? 0x80 : 0x00));
}

static void buf_u16be(Buf *b, uint16_t v) {
    buf_push(b, v >> 8); buf_push(b, v & 0xFF);
}

static void buf_u32be(Buf *b, uint32_t v) {
    buf_push(b, v >> 24); buf_push(b, (v >> 16) & 0xFF);
    buf_push(b, (v >>  8) & 0xFF); buf_push(b,  v & 0xFF);
}

/* ---- Event list ---- */

typedef struct { uint32_t tick; uint8_t d[4]; int len; } Ev;

#define MAX_EVENTS 1024
static Ev events[MAX_EVENTS];
static int nevents = 0;

static void ev(uint32_t tick, uint8_t a, uint8_t b, uint8_t c) {
    events[nevents++] = (Ev){ tick, {a,b,c,0}, 3 };
}
static void ev2(uint32_t tick, uint8_t a, uint8_t b) {
    events[nevents++] = (Ev){ tick, {a,b,0,0}, 2 };
}
static void ev_meta(uint32_t tick, uint8_t type, uint8_t *data, int dlen) {
    Ev e; e.tick = tick;
    e.d[0] = 0xFF; e.d[1] = type; e.d[2] = (uint8_t)dlen; e.len = 3 + dlen;
    /* store extra bytes after — only for small metas we use here (≤1 extra) */
    (void)data; /* handled inline below for tempo */
    events[nevents++] = e;
}

static int ev_cmp(const void *a, const void *b) {
    return (int)((const Ev*)a)->tick - (int)((const Ev*)b)->tick;
}

/* ---- Note helpers ---- */
#define TPB 480   /* ticks per beat */
#define Q   TPB
#define H   (TPB*2)
#define W   (TPB*4)
#define E   (TPB/2)

/* note on / note off with small gap for articulation */
static void note(uint32_t t, int ch, int n, int dur) {
    ev(t,           0x90|ch, n, 96);
    ev(t + dur - E, 0x80|ch, n,  0);
}
static void bass_note(uint32_t t, int ch, int n, int dur) {
    ev(t,           0x90|ch, n, 72);
    ev(t + dur - Q, 0x80|ch, n,  0);
}

int main(void) {
    /* Instruments: ch0 = Flute (73), ch1 = Pizzicato Strings (45) */
    ev2(0, 0xC0, 73);   /* ch0: Flute */
    ev2(0, 0xC1, 45);   /* ch1: Pizzicato Strings */

    /* Tempo: 110 BPM = 545454 us/beat */
    /* FF 51 03 xx xx xx — store as raw 6-byte meta */
    uint32_t t0 = 0;
    events[nevents++] = (Ev){ t0, {0xFF, 0x51, 0x03, 0}, 3 };
    /* We need 3 more bytes for tempo — pack a second fake event and splice later.
       Instead, handle tempo specially: store all 6 bytes inline using len=6 */
    nevents--; /* undo */
    Ev te; te.tick = 0;
    te.d[0]=0xFF; te.d[1]=0x51; te.d[2]=0x03; te.len = 6;
    /* store 3 tempo bytes in remaining space */
    uint32_t us = 545454;
    /* We only have 4 bytes in d[], so borrow a trick: store raw after d[3] */
    /* Actually let's just write the tempo manually into the track below */
    (void)te;

    /* MIDI notes (C major):
       C3=48 D3=50 E3=52 F3=53 G3=55 A3=57
       C4=60 D4=62 E4=64 F4=65 G4=67 A4=69 B4=71
       C5=72 D5=74 E5=76 G5=79 A5=81               */

    /* ---- 8-bar melody (Flute, ch0) ---- */
    uint32_t t = 0;

    /* Bar 1: C E G A — ascending lift */
    note(t,     0, 60, Q); t+=Q;
    note(t,     0, 64, Q); t+=Q;
    note(t,     0, 67, Q); t+=Q;
    note(t,     0, 69, Q); t+=Q;
    /* Bar 2: G.(dotted half) E */
    note(t,     0, 67, Q+H); t+=Q+H;
    note(t,     0, 64, Q);   t+=Q;
    /* Bar 3: D E G E */
    note(t,     0, 62, Q); t+=Q;
    note(t,     0, 64, Q); t+=Q;
    note(t,     0, 67, Q); t+=Q;
    note(t,     0, 64, Q); t+=Q;
    /* Bar 4: C whole */
    note(t,     0, 60, W); t+=W;
    /* Bar 5: A G E G */
    note(t,     0, 69, Q); t+=Q;
    note(t,     0, 67, Q); t+=Q;
    note(t,     0, 64, Q); t+=Q;
    note(t,     0, 67, Q); t+=Q;
    /* Bar 6: C5 A G E */
    note(t,     0, 72, Q); t+=Q;
    note(t,     0, 69, Q); t+=Q;
    note(t,     0, 67, Q); t+=Q;
    note(t,     0, 64, Q); t+=Q;
    /* Bar 7: F G A G */
    note(t,     0, 65, Q); t+=Q;
    note(t,     0, 67, Q); t+=Q;
    note(t,     0, 69, Q); t+=Q;
    note(t,     0, 67, Q); t+=Q;
    /* Bar 8: E(half) C(half) */
    note(t,     0, 64, H); t+=H;
    note(t,     0, 60, H); t+=H;

    uint32_t loopLen = t; /* 8 bars */

    /* ---- 8-bar bass (Pizzicato, ch1) ---- */
    t = 0;
    /* Bar 1: C3 half, C3 half */
    bass_note(t, 1, 48, H); t+=H;
    bass_note(t, 1, 48, H); t+=H;
    /* Bar 2: G3 half, G3 half */
    bass_note(t, 1, 55, H); t+=H;
    bass_note(t, 1, 55, H); t+=H;
    /* Bar 3: A3 half, E3 half */
    bass_note(t, 1, 57, H); t+=H;
    bass_note(t, 1, 52, H); t+=H;
    /* Bar 4: C3 whole */
    bass_note(t, 1, 48, W); t+=W;
    /* Bar 5: F3 half, G3 half */
    bass_note(t, 1, 53, H); t+=H;
    bass_note(t, 1, 55, H); t+=H;
    /* Bar 6: F3 half, G3 half */
    bass_note(t, 1, 53, H); t+=H;
    bass_note(t, 1, 55, H); t+=H;
    /* Bar 7: F3 half, G3 half */
    bass_note(t, 1, 53, H); t+=H;
    bass_note(t, 1, 55, H); t+=H;
    /* Bar 8: C3 whole */
    bass_note(t, 1, 48, W); t+=W;

    /* End of track */
    events[nevents++] = (Ev){ loopLen, {0xFF, 0x2F, 0x00, 0}, 3 };

    qsort(events, nevents, sizeof(Ev), ev_cmp);

    /* ---- Build track chunk ---- */
    Buf trk = {0};

    /* Tempo meta event: FF 51 03 08 52 EE (545454 us) */
    buf_varlen(&trk, 0);        /* delta time = 0 */
    buf_push(&trk, 0xFF);
    buf_push(&trk, 0x51);
    buf_push(&trk, 0x03);
    buf_push(&trk, (us >> 16) & 0xFF);
    buf_push(&trk, (us >>  8) & 0xFF);
    buf_push(&trk,  us        & 0xFF);

    /* Write all other events with delta times */
    uint32_t prev = 0;
    for (int i = 0; i < nevents; i++) {
        /* skip tempo placeholder we handled above (FF 51) */
        if (events[i].d[0] == 0xFF && events[i].d[1] == 0x51) continue;
        uint32_t dt = events[i].tick - prev;
        prev = events[i].tick;
        buf_varlen(&trk, dt);
        for (int j = 0; j < events[i].len; j++)
            buf_push(&trk, events[i].d[j]);
    }

    /* ---- Write MIDI file ---- */
    FILE *f = fopen("assets/world_music.mid", "wb");
    if (!f) { fprintf(stderr, "Cannot write assets/world_music.mid\n"); return 1; }

    Buf hdr = {0};
    /* MThd */
    for (const char *c = "MThd"; *c; c++) buf_push(&hdr, *c);
    buf_u32be(&hdr, 6);          /* chunk length */
    buf_u16be(&hdr, 0);          /* format 0 */
    buf_u16be(&hdr, 1);          /* 1 track */
    buf_u16be(&hdr, TPB);        /* ticks per beat */
    fwrite(hdr.data, 1, hdr.len, f);

    /* MTrk */
    fwrite("MTrk", 1, 4, f);
    uint32_t tlen = (uint32_t)trk.len;
    uint8_t tlen_be[4] = { tlen>>24, (tlen>>16)&0xFF, (tlen>>8)&0xFF, tlen&0xFF };
    fwrite(tlen_be, 1, 4, f);
    fwrite(trk.data, 1, trk.len, f);

    fclose(f);
    free(hdr.data);
    free(trk.data);
    printf("Generated assets/world_music.mid (%zu track bytes)\n", trk.len);
    return 0;
}
