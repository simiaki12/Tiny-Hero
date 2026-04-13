/* audio.c — procedural PCM synthesis via waveOut.
   Works reliably in Wine (maps to ALSA/PulseAudio) unlike MCI MIDI.
   Two-voice triangle-wave synth: melody (flute-like) + bass (plucked). */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <string.h>
#include "audio.h"

/* --- Audio format --- */
#define SR    22050   /* sample rate */
#define BUFSZ 4096    /* samples per waveOut buffer (~185ms each) */
#define NBUFS 2       /* double-buffer */

/* --- Note sequence (110 BPM, C major) --- */
/* Quarter note = SR*60/110 = 12027 samples */
#define QN 12027
#define HN (QN*2)
#define WN (QN*4)
#define DH (QN*3)   /* dotted half */

typedef struct { float f; int d; } Note; /* frequency Hz, duration samples */

/* Frequencies for the notes we use */
#define C3f 130.81f
#define D3f 146.83f
#define E3f 164.81f
#define F3f 174.61f
#define G3f 196.00f
#define A3f 220.00f
#define C4f 261.63f
#define D4f 293.66f
#define E4f 329.63f
#define F4f 349.23f
#define G4f 392.00f
#define A4f 440.00f
#define C5f 523.25f

/* 8-bar melody */
static const Note s_mel[] = {
    {C4f,QN},{E4f,QN},{G4f,QN},{A4f,QN},   /* bar 1: ascending lift    */
    {G4f,DH},{E4f,QN},                       /* bar 2: dotted-half peak  */
    {D4f,QN},{E4f,QN},{G4f,QN},{E4f,QN},   /* bar 3: bounce            */
    {C4f,WN},                                /* bar 4: resolve           */
    {A4f,QN},{G4f,QN},{E4f,QN},{G4f,QN},   /* bar 5: variation         */
    {C5f,QN},{A4f,QN},{G4f,QN},{E4f,QN},   /* bar 6: descend from high */
    {F4f,QN},{G4f,QN},{A4f,QN},{G4f,QN},   /* bar 7: build-back        */
    {E4f,HN},{C4f,HN},                       /* bar 8: final resolve     */
};
#define MEL_N (int)(sizeof(s_mel)/sizeof(s_mel[0]))

/* 8-bar bass line (matches melody loop length exactly) */
static const Note s_bas[] = {
    {C3f,HN},{C3f,HN},
    {G3f,HN},{G3f,HN},
    {A3f,HN},{E3f,HN},
    {C3f,WN},
    {F3f,HN},{G3f,HN},
    {F3f,HN},{G3f,HN},
    {F3f,HN},{G3f,HN},
    {C3f,WN},
};
#define BAS_N (int)(sizeof(s_bas)/sizeof(s_bas[0]))

/* --- Voice oscillator --- */
typedef struct {
    int   ni;   /* note index   */
    int   np;   /* sample pos within note */
    float ph;   /* oscillator phase [0,1) */
} Voice;

static Voice s_mv, s_bv;

static void voice_reset(Voice *v) { v->ni = 0; v->np = 0; v->ph = 0.0f; }

static float voice_tick(Voice *v, const Note *seq, int n) {
    const Note *nt = &seq[v->ni];
    float s = 0.0f;
    /* gate: note sounds for 80% of its duration, then rests (articulation) */
    if (v->np < (int)(nt->d * 0.80f) && nt->f > 0.0f) {
        v->ph += nt->f / (float)SR;
        if (v->ph >= 1.0f) v->ph -= 1.0f;
        /* triangle wave — softer than square, no math.h needed */
        float p = v->ph - 0.5f;
        s = 1.0f - 4.0f * (p < 0.0f ? -p : p);
    }
    if (++v->np >= nt->d) { v->np = 0; v->ni = (v->ni + 1) % n; }
    return s;
}

/* --- waveOut streaming --- */
static HWAVEOUT       s_wo     = NULL;
static HANDLE         s_thread = NULL;
static HANDLE         s_evt    = NULL;
static volatile LONG  s_stop   = 1;
static int16_t        s_pcm[NBUFS][BUFSZ];
static WAVEHDR        s_hdr[NBUFS];

static void fill(int16_t *buf, int n) {
    for (int i = 0; i < n; i++) {
        float v = voice_tick(&s_mv, s_mel, MEL_N) * 0.52f
                + voice_tick(&s_bv, s_bas, BAS_N) * 0.32f;
        if (v >  1.0f) v =  1.0f;
        if (v < -1.0f) v = -1.0f;
        buf[i] = (int16_t)(v * 26000);
    }
}

static DWORD WINAPI audio_thread(LPVOID arg) {
    (void)arg;
    while (!s_stop) {
        WaitForSingleObject(s_evt, 200);
        for (int i = 0; i < NBUFS; i++) {
            if (!s_stop && (s_hdr[i].dwFlags & WHDR_DONE)) {
                waveOutUnprepareHeader(s_wo, &s_hdr[i], sizeof(WAVEHDR));
                fill(s_pcm[i], BUFSZ);
                waveOutPrepareHeader(s_wo, &s_hdr[i], sizeof(WAVEHDR));
                waveOutWrite(s_wo, &s_hdr[i], sizeof(WAVEHDR));
            }
        }
    }
    return 0;
}

void audioPlayWorld(void) {
    if (s_wo) return;

    WAVEFORMATEX fmt;
    fmt.wFormatTag      = WAVE_FORMAT_PCM;
    fmt.nChannels       = 1;
    fmt.nSamplesPerSec  = SR;
    fmt.nAvgBytesPerSec = SR * 2;
    fmt.nBlockAlign     = 2;
    fmt.wBitsPerSample  = 16;
    fmt.cbSize          = 0;

    s_evt = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!s_evt) return;

    if (waveOutOpen(&s_wo, WAVE_MAPPER, &fmt,
                    (DWORD_PTR)s_evt, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        CloseHandle(s_evt); s_evt = NULL; return;
    }

    voice_reset(&s_mv);
    voice_reset(&s_bv);
    InterlockedExchange(&s_stop, 0);

    for (int i = 0; i < NBUFS; i++) {
        memset(&s_hdr[i], 0, sizeof(WAVEHDR));
        s_hdr[i].lpData         = (LPSTR)s_pcm[i];
        s_hdr[i].dwBufferLength = BUFSZ * 2;
        fill(s_pcm[i], BUFSZ);
        waveOutPrepareHeader(s_wo, &s_hdr[i], sizeof(WAVEHDR));
        waveOutWrite(s_wo, &s_hdr[i], sizeof(WAVEHDR));
    }

    s_thread = CreateThread(NULL, 0, audio_thread, NULL, 0, NULL);
}

void audioStop(void) {
    if (!s_wo) return;
    InterlockedExchange(&s_stop, 1);
    if (s_evt) SetEvent(s_evt);
    if (s_thread) {
        WaitForSingleObject(s_thread, 3000);
        CloseHandle(s_thread);
        s_thread = NULL;
    }
    waveOutReset(s_wo);
    for (int i = 0; i < NBUFS; i++)
        waveOutUnprepareHeader(s_wo, &s_hdr[i], sizeof(WAVEHDR));
    waveOutClose(s_wo);
    s_wo = NULL;
    if (s_evt) { CloseHandle(s_evt); s_evt = NULL; }
}

void audioCleanup(void) { audioStop(); }
