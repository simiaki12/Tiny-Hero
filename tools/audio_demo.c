/* audio_demo.c — synthesizer sampler for TinyHero audio engine.
   Writes 23 WAV files covering every waveform, ADSR shape, and pattern
   the engine supports, so a composer can hear the full sonic palette.

   Replicates voice_tick / ADSR / oscillator logic exactly from audio.c.
   No Windows headers, no math.h.

   Build:  gcc -std=c11 -O2 -o build/audio_demo tools/audio_demo.c
   Or:     make audio_demo
   Run:    ./build/audio_demo          (writes demo_*.wav here) */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Engine constants (match audio.c) ── */
#define SR  22050
#define QN  12027           /* quarter note @110 BPM */
#define HN  (QN*2)
#define WN  (QN*4)
#define DH  (QN*3)
#define EN  (QN/2)
#define SN  (QN/4)

typedef struct { float f; int d; } Note;

/* ── Frequencies ── */
#define C2f   65.41f
#define C3f  130.81f
#define D3f  146.83f
#define E3f  164.81f
#define F3f  174.61f
#define G3f  196.00f
#define A3f  220.00f
#define B3f  246.94f
#define C4f  261.63f
#define D4f  293.66f
#define E4f  329.63f
#define F4f  349.23f
#define G4f  392.00f
#define A4f  440.00f
#define B4f  493.88f
#define C5f  523.25f
#define D5f  587.33f

/* ── ADSR state machine ── */
typedef enum { ENV_ATK, ENV_DCY, ENV_SUS, ENV_REL, ENV_OFF } EnvPhase;

typedef struct {
    int      ni, np;
    float    ph;
    float    env;
    EnvPhase ep;
    float    atk, dcy, sus, rel, gate_frac;
    int      wave;
} Voice;

static uint32_t s_nsr;
static void     noise_reset(void) { s_nsr = 0xCAFEBABEu; }
static float    noise_next(void) {
    s_nsr ^= s_nsr << 13;
    s_nsr ^= s_nsr >> 17;
    s_nsr ^= s_nsr <<  5;
    return (int32_t)s_nsr / 2147483648.0f;
}

static void voice_init(Voice *v, int wave,
                       float atk, float dcy, float sus, float rel,
                       float gate_frac) {
    memset(v, 0, sizeof(*v));
    v->wave = wave; v->atk = atk; v->dcy = dcy;
    v->sus  = sus;  v->rel = rel; v->gate_frac = gate_frac;
    v->ep   = ENV_ATK;
}

static float voice_tick(Voice *v, const Note *seq, int n) {
    const Note *nt = &seq[v->ni];

    if (v->np == (int)(nt->d * v->gate_frac) &&
        v->ep != ENV_REL && v->ep != ENV_OFF)
        v->ep = ENV_REL;

    switch (v->ep) {
        case ENV_ATK:
            v->env += v->atk;
            if (v->env >= 1.0f) { v->env = 1.0f; v->ep = ENV_DCY; }
            break;
        case ENV_DCY:
            v->env -= v->dcy;
            if (v->env <= v->sus) { v->env = v->sus; v->ep = ENV_SUS; }
            break;
        case ENV_SUS: break;
        case ENV_REL:
            v->env -= v->rel;
            if (v->env <= 0.0f) { v->env = 0.0f; v->ep = ENV_OFF; }
            break;
        case ENV_OFF: break;
    }

    float s = 0.0f;
    if (v->ep != ENV_OFF && nt->f > 0.0f) {
        if (v->wave == 3) {
            s = noise_next();
        } else {
            v->ph += nt->f / (float)SR;
            if (v->ph >= 1.0f) v->ph -= 1.0f;
            switch (v->wave) {
                case 0: { float p = v->ph - 0.5f; s = 1.0f - 4.0f*(p<0.0f?-p:p); break; }
                case 1: s = v->ph < 0.5f ?  1.0f : -1.0f; break;
                case 2: s = 2.0f * v->ph - 1.0f; break;
            }
        }
        s *= v->env;
    }

    if (++v->np >= nt->d) {
        v->np  = 0;
        v->ni  = (v->ni + 1) % n;
        v->ph  = 0.0f;
        v->env = 0.0f;
        v->ep  = ENV_ATK;
    }
    return s;
}

/* ── WAV writer ── */
static void wu16(FILE *f, uint16_t v) {
    uint8_t b[2] = { v & 0xFF, (v>>8) & 0xFF };
    fwrite(b, 1, 2, f);
}
static void wu32(FILE *f, uint32_t v) {
    uint8_t b[4] = { v&0xFF, (v>>8)&0xFF, (v>>16)&0xFF, (v>>24)&0xFF };
    fwrite(b, 1, 4, f);
}
static void wav_header(FILE *f, int n) {
    uint32_t db = (uint32_t)n * 2;
    fwrite("RIFF", 1, 4, f); wu32(f, 36+db);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); wu32(f,16); wu16(f,1); wu16(f,1);
    wu32(f,SR); wu32(f,SR*2); wu16(f,2); wu16(f,16);
    fwrite("data",1,4,f); wu32(f,db);
}
static void ws16(FILE *f, float v) {
    if (v >  1.0f) v =  1.0f;
    if (v < -1.0f) v = -1.0f;
    int16_t s = (int16_t)(v * 26000.0f);
    uint8_t b[2] = { (uint8_t)(s & 0xFF), (uint8_t)((s>>8) & 0xFF) };
    fwrite(b, 1, 2, f);
}

/* ── Sequence definitions ── */

/* World theme — copied verbatim from audio.c */
static const Note s_mel[] = {
    {C4f,QN},{E4f,QN},{G4f,QN},{A4f,QN},
    {G4f,DH},{E4f,QN},
    {D4f,QN},{E4f,QN},{G4f,QN},{E4f,QN},
    {C4f,WN},
    {A4f,QN},{G4f,QN},{E4f,QN},{G4f,QN},
    {C5f,QN},{A4f,QN},{G4f,QN},{E4f,QN},
    {F4f,QN},{G4f,QN},{A4f,QN},{G4f,QN},
    {E4f,HN},{C4f,HN},
};
#define MEL_N (int)(sizeof(s_mel)/sizeof(s_mel[0]))

static const Note s_har[] = {
    {E4f,WN},{G4f,WN},{E4f,HN},{C4f,HN},{E4f,WN},
    {C4f,WN},{D4f,WN},{C4f,WN},{G4f,WN},
};
#define HAR_N (int)(sizeof(s_har)/sizeof(s_har[0]))

static const Note s_bas[] = {
    {C3f,HN},{C3f,HN},{G3f,HN},{G3f,HN},
    {A3f,HN},{E3f,HN},{C3f,WN},
    {F3f,HN},{G3f,HN},{F3f,HN},{G3f,HN},
    {F3f,HN},{G3f,HN},{C3f,WN},
};
#define BAS_N (int)(sizeof(s_bas)/sizeof(s_bas[0]))

/* 8-bar drum loop (matches 8 bars of the world theme) */
static const Note s_drm8[] = {
    {0.0f,QN},{1.0f,QN},{0.0f,QN},{1.0f,QN},
    {0.0f,QN},{1.0f,QN},{0.0f,QN},{1.0f,QN},
    {0.0f,QN},{1.0f,QN},{0.0f,QN},{1.0f,QN},
    {0.0f,QN},{1.0f,QN},{0.0f,QN},{1.0f,QN},
    {0.0f,QN},{1.0f,QN},{0.0f,QN},{1.0f,QN},
    {0.0f,QN},{1.0f,QN},{0.0f,QN},{1.0f,QN},
    {0.0f,QN},{1.0f,QN},{0.0f,QN},{1.0f,QN},
    {0.0f,QN},{1.0f,QN},{0.0f,QN},{1.0f,QN},
};
#define DRM8_N (int)(sizeof(s_drm8)/sizeof(s_drm8[0]))

/* C major scale up + down */
static const Note s_scale[] = {
    {C4f,QN},{D4f,QN},{E4f,QN},{F4f,QN},
    {G4f,QN},{A4f,QN},{B4f,QN},{C5f,QN},
    {B4f,QN},{A4f,QN},{G4f,QN},{F4f,QN},
    {E4f,QN},{D4f,QN},{C4f,HN},
};
#define SCALE_N (int)(sizeof(s_scale)/sizeof(s_scale[0]))

/* Staccato sixteenth-note run */
static const Note s_staccato[] = {
    {C4f,EN},{D4f,EN},{E4f,EN},{G4f,EN},
    {E4f,EN},{G4f,EN},{A4f,EN},{G4f,EN},
    {C5f,EN},{A4f,EN},{G4f,EN},{E4f,EN},
    {D4f,EN},{E4f,EN},{C4f,HN},
};
#define STACCATO_N (int)(sizeof(s_staccato)/sizeof(s_staccato[0]))

/* Arpeggio: C–Am–F–G */
static const Note s_arp[] = {
    {C4f,EN},{E4f,EN},{G4f,EN},{C5f,EN},{G4f,EN},{E4f,EN},{C4f,QN},
    {A3f,EN},{C4f,EN},{E4f,EN},{A4f,EN},{E4f,EN},{C4f,EN},{A3f,QN},
    {F3f,EN},{A3f,EN},{C4f,EN},{F4f,EN},{C4f,EN},{A3f,EN},{F3f,QN},
    {G3f,EN},{B3f,EN},{D4f,EN},{G4f,EN},{D4f,EN},{B3f,EN},{G3f,QN},
};
#define ARP_N (int)(sizeof(s_arp)/sizeof(s_arp[0]))

/* A natural minor scale */
static const Note s_minor[] = {
    {A3f,QN},{B3f,QN},{C4f,QN},{D4f,QN},
    {E4f,QN},{F4f,QN},{G4f,QN},{A4f,QN},
    {G4f,QN},{F4f,QN},{E4f,QN},{D4f,QN},
    {C4f,QN},{B3f,QN},{A3f,HN},
};
#define MINOR_N (int)(sizeof(s_minor)/sizeof(s_minor[0]))

/* Sixteenth-note noise roll */
static const Note s_roll[] = {
    {1.0f,SN},{0.0f,SN},{1.0f,SN},{0.0f,SN},
    {1.0f,SN},{1.0f,SN},{0.0f,SN},{0.0f,SN},
    {1.0f,SN},{0.0f,SN},{1.0f,SN},{1.0f,SN},
    {0.0f,SN},{0.0f,SN},{1.0f,SN},{1.0f,SN},
};
#define ROLL_N (int)(sizeof(s_roll)/sizeof(s_roll[0]))

/* Slow cymbal hits */
static const Note s_cymbal[] = {
    {1.0f,HN},{0.0f,HN},{1.0f,HN},{0.0f,HN},{1.0f,WN},{0.0f,HN},
};
#define CYMBAL_N (int)(sizeof(s_cymbal)/sizeof(s_cymbal[0]))

/* ── Single-voice writer ── */
#define MAX_PAD 1024

static void write_single(const char *path, int wave,
                         float atk, float dcy, float sus, float rel,
                         float gate_frac,
                         const Note *seq, int n, float vol) {
    /* Append 2-second rest so the envelope decays to silence. */
    static Note pad[MAX_PAD];
    int pn = (n < MAX_PAD - 1) ? n : MAX_PAD - 1;
    for (int i = 0; i < pn; i++) pad[i] = seq[i];
    pad[pn] = (Note){0.0f, SR * 2};
    pn++;

    int total = 0;
    for (int i = 0; i < pn; i++) total += pad[i].d;

    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return; }
    wav_header(f, total);

    Voice v;
    voice_init(&v, wave, atk, dcy, sus, rel, gate_frac);
    noise_reset();
    for (int i = 0; i < total; i++)
        ws16(f, voice_tick(&v, pad, pn) * vol);

    fclose(f);
    printf("  %-48s  %.1fs\n", path, total / (float)SR);
}

/* ── Multi-voice mix writer ── */
typedef struct {
    int   wave;
    float atk, dcy, sus, rel, gate_frac, vol;
    const Note *seq;
    int   n;
} VDef;

static void write_mix(const char *path, VDef *vd, int nv) {
    static Note pads[4][MAX_PAD];
    static int  pns[4];

    int total = 0;
    for (int vi = 0; vi < nv && vi < 4; vi++) {
        int t = 0;
        for (int i = 0; i < vd[vi].n; i++) t += vd[vi].seq[i].d;
        int pn = (vd[vi].n < MAX_PAD - 1) ? vd[vi].n : MAX_PAD - 1;
        for (int i = 0; i < pn; i++) pads[vi][i] = vd[vi].seq[i];
        pads[vi][pn] = (Note){0.0f, SR * 2};
        pns[vi] = pn + 1;
        t += SR * 2;
        if (t > total) total = t;
    }

    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return; }
    wav_header(f, total);

    Voice vs[4];
    for (int vi = 0; vi < nv && vi < 4; vi++)
        voice_init(&vs[vi], vd[vi].wave,
                   vd[vi].atk, vd[vi].dcy, vd[vi].sus,
                   vd[vi].rel, vd[vi].gate_frac);
    noise_reset();

    for (int i = 0; i < total; i++) {
        float mix = 0.0f;
        for (int vi = 0; vi < nv && vi < 4; vi++)
            mix += voice_tick(&vs[vi], pads[vi], pns[vi]) * vd[vi].vol;
        ws16(f, mix);
    }

    fclose(f);
    printf("  %-48s  %.1fs\n", path, total / (float)SR);
}

/* ── Main ── */
int main(void) {
    printf("TinyHero audio demo — writing WAV files...\n");

    /* ── 1. Waveform comparison — same melody, same neutral envelope, different timbre ── */
    printf("\n[Waveforms] world melody in each waveform, neutral envelope\n");
    write_single("demo_01_triangle_melody.wav", 0,
        1.0f/(SR*0.010f), 1.0f/(SR*0.30f), 0.80f, 1.0f/(SR*0.15f), 0.95f,
        s_mel, MEL_N, 0.70f);
    write_single("demo_02_square_melody.wav",   1,
        1.0f/(SR*0.010f), 1.0f/(SR*0.30f), 0.80f, 1.0f/(SR*0.15f), 0.95f,
        s_mel, MEL_N, 0.60f);
    write_single("demo_03_saw_melody.wav",      2,
        1.0f/(SR*0.010f), 1.0f/(SR*0.30f), 0.80f, 1.0f/(SR*0.15f), 0.95f,
        s_mel, MEL_N, 0.55f);
    Note noise1s[] = {{1.0f, SR}};
    write_single("demo_04_noise_raw.wav",   3,
        1.0f, 0.0f, 1.0f, 0.002f, 1.0f,
        noise1s, 1, 0.40f);

    /* ── 2. ADSR shapes — all triangle, C4 ── */
    printf("\n[ADSR shapes] triangle wave, C4, different envelope characters\n");

    /* percussive: instant attack, fast decay, no sustain */
    Note c4_qn[] = {{C4f,QN},{E4f,QN},{G4f,QN},{C5f,QN},{G4f,QN},{E4f,QN},{C4f,HN}};
    write_single("demo_05_percussive.wav", 0,
        1.0f, 1.0f/(SR*0.080f), 0.0f, 0.002f, 1.0f,
        c4_qn, 7, 0.70f);

    /* plucked: instant attack, medium decay to 0 (bass-guitar feel) */
    write_single("demo_06_plucked.wav", 0,
        1.0f, 1.0f/(QN*1.5f), 0.0f, 0.002f, 1.0f,
        s_scale, SCALE_N, 0.70f);

    /* flute: 15ms attack, 500ms decay, high sustain — world melody voice */
    write_single("demo_07_flute_melody.wav", 0,
        1.0f/(SR*0.015f), 1.0f/(SR*0.50f), 0.75f, 0.75f/(SR*0.05f), 0.85f,
        s_mel, MEL_N, 0.70f);

    /* slow pad: 200ms attack, 3s decay, high sustain */
    Note c4_long[] = {{C4f, WN*2}};
    write_single("demo_08_pad_slow_attack.wav", 0,
        1.0f/(SR*0.20f), 1.0f/(SR*3.00f), 0.75f, 1.0f/(SR*0.30f), 0.90f,
        c4_long, 1, 0.70f);

    /* staccato: gate at 30%, very short articulation */
    write_single("demo_09_staccato.wav", 1,
        1.0f/(SR*0.005f), 1.0f/(SR*0.05f), 0.70f, 1.0f/(SR*0.03f), 0.30f,
        s_staccato, STACCATO_N, 0.60f);

    /* square pad: 80ms attack, 2s decay, 55% sustain — world harmony voice */
    write_single("demo_10_square_pad.wav", 1,
        1.0f/(SR*0.080f), 1.0f/(SR*2.00f), 0.55f, 0.55f/(SR*0.06f), 0.88f,
        s_har, HAR_N, 0.60f);

    /* ── 3. Melodic patterns ── */
    printf("\n[Melodic patterns] scales, arpeggios, minor mode\n");

    /* triangle with game flute envelope (world melody voice) */
    write_single("demo_11_triangle_flute_env.wav", 0,
        1.0f/(SR*0.015f), 1.0f/(SR*0.50f), 0.75f, 0.75f/(SR*0.05f), 0.85f,
        s_mel, MEL_N, 0.70f);

    /* square with game pad envelope (world harmony voice) */
    write_single("demo_12_square_pad_env.wav", 1,
        1.0f/(SR*0.080f), 1.0f/(SR*2.00f), 0.55f, 0.55f/(SR*0.06f), 0.88f,
        s_mel, MEL_N, 0.60f);

    /* saw with a bright lead envelope */
    write_single("demo_13_saw_lead_env.wav", 2,
        1.0f/(SR*0.005f), 1.0f/(SR*0.10f), 0.65f, 1.0f/(SR*0.04f), 0.90f,
        s_mel, MEL_N, 0.50f);

    write_single("demo_14_arpeggio_triangle.wav", 0,
        1.0f/(SR*0.008f), 1.0f/(SR*0.15f), 0.65f, 1.0f/(SR*0.03f), 0.80f,
        s_arp, ARP_N, 0.70f);

    write_single("demo_15_minor_scale.wav", 0,
        1.0f/(SR*0.015f), 1.0f/(SR*0.20f), 0.70f, 1.0f/(SR*0.05f), 0.85f,
        s_minor, MINOR_N, 0.70f);

    /* ── 4. Percussion & noise ── */
    printf("\n[Percussion] noise waveform, different decay lengths\n");

    /* hi-hat: fast noise burst (40ms decay) — world percussion voice */
    write_single("demo_16_hihat.wav", 3,
        1.0f, 1.0f/(SR*0.040f), 0.0f, 0.010f, 1.0f,
        s_drm8, DRM8_N, 0.50f);

    /* cymbal: slower noise decay (350ms) */
    write_single("demo_17_cymbal.wav", 3,
        1.0f, 1.0f/(SR*0.350f), 0.0f, 0.005f, 1.0f,
        s_cymbal, CYMBAL_N, 0.50f);

    /* 16th-note noise roll */
    write_single("demo_18_noise_roll.wav", 3,
        1.0f, 1.0f/(SR*0.020f), 0.0f, 0.010f, 1.0f,
        s_roll, ROLL_N, 0.50f);

    /* ── 5. Bass textures ── */
    printf("\n[Bass] low register, different articulation\n");

    /* plucked bass (world bass voice) */
    write_single("demo_19_bass_plucked.wav", 0,
        1.0f, 1.0f/(QN*1.5f), 0.0f, 0.002f, 1.0f,
        s_bas, BAS_N, 0.80f);

    /* sustained bass */
    write_single("demo_20_bass_sustained.wav", 0,
        1.0f/(SR*0.020f), 1.0f/(SR*0.50f), 0.60f, 1.0f/(SR*0.10f), 0.85f,
        s_bas, BAS_N, 0.75f);

    /* sawtooth bass */
    write_single("demo_21_bass_saw.wav", 2,
        1.0f/(SR*0.010f), 1.0f/(SR*0.30f), 0.50f, 1.0f/(SR*0.10f), 0.85f,
        s_bas, BAS_N, 0.65f);

    /* ── 6. Chord pad — C–Am–F–G progression, each chord one whole note ── */
    printf("\n[Chord pad] 3-voice square pad moving through C–Am–F–G\n");
    /* Each voice plays the progression; the 3 voices are the chord tones.
       C: C-E-G   Am: A-C-E   F: F-A-C   G: G-B-D
       Root voice: C4 → A3 → F3 → G3 */
    static const Note pad_root[] = {{C4f,WN},{A3f,WN},{F3f,WN},{G3f,WN}};
    static const Note pad_mid[]  = {{E4f,WN},{C4f,WN},{A3f,WN},{B3f,WN}};
    static const Note pad_top[]  = {{G4f,WN},{E4f,WN},{C4f,WN},{D4f,WN}};
    VDef pad3[] = {
        { 1, 1.0f/(SR*0.10f), 1.0f/(SR*2.5f), 0.60f, 1.0f/(SR*0.30f), 0.90f,
          0.30f, pad_root, 4 },
        { 1, 1.0f/(SR*0.12f), 1.0f/(SR*2.5f), 0.60f, 1.0f/(SR*0.30f), 0.90f,
          0.30f, pad_mid,  4 },
        { 1, 1.0f/(SR*0.14f), 1.0f/(SR*2.5f), 0.60f, 1.0f/(SR*0.30f), 0.90f,
          0.30f, pad_top,  4 },
    };
    write_mix("demo_22_chord_pad_progression.wav", pad3, 3);

    /* ── 7. Full world theme (exact game mix weights) ── */
    printf("\n[Full mix] world theme — all 4 voices, game mix weights\n");
    VDef world[] = {
        /* melody: triangle, flute (×0.40) */
        { 0, 1.0f/(SR*0.015f), 1.0f/(SR*0.50f), 0.75f, 0.75f/(SR*0.05f), 0.85f,
          0.40f, s_mel, MEL_N },
        /* harmony: square pad (×0.12) */
        { 1, 1.0f/(SR*0.080f), 1.0f/(SR*2.00f), 0.55f, 0.55f/(SR*0.06f), 0.88f,
          0.12f, s_har, HAR_N },
        /* bass: triangle, plucked (×0.25) */
        { 0, 1.0f, 1.0f/(QN*1.5f), 0.0f, 0.002f, 1.0f,
          0.25f, s_bas, BAS_N },
        /* percussion: noise hi-hat (×0.07) */
        { 3, 1.0f, 1.0f/(SR*0.040f), 0.0f, 0.010f, 1.0f,
          0.07f, s_drm8, DRM8_N },
    };
    write_mix("demo_23_world_theme_full.wav", world, 4);

    printf("\nDone. 23 files written.\n");
    return 0;
}
