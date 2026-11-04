// Engine-level tests for Millstone: drives mpc_engine() directly (no VST wrapper), checks every model
// makes sound without NaNs, voice modes, VCA modes, filter modes, state round-trip, voice freeing,
// and times the worst case (8 voices x 3 oscillators) to estimate the CPU budget.
// Also writes a short demo WAV (first argument) so a human can listen.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
extern "C" {
#include "engine.h"
}
#include "params.h"

// Every allocation starts as a huge float. Plaits' ChiptuneEngine::Init leaves its last oscillator uninitialised, and at a
// phase this size subtracting 1 changes nothing, so that oscillator's loop never ends and the audio thread freezes. Real
// leftover memory did this at random. The engine must never see such memory.
#include <new>
void *operator new(size_t n, const std::nothrow_t &) noexcept { void *p = malloc(n ? n : 1); if (p) memset(p, 0x7f, n); return p; }
void *operator new(size_t n) { void *p = malloc(n ? n : 1); if (!p) abort(); memset(p, 0x7f, n); return p; }
void operator delete(void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }

static int fails = 0;
#define CHECK(c, ...) do { printf("%s ", (c) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)

static const mpc_engine_t *E;

static void note(void *in, int n, int vel) { uint8_t m[3] = {0x90, (uint8_t)n, (uint8_t)vel}; E->midi(in, m, 3); }
static void off(void *in, int n) { uint8_t m[3] = {0x80, (uint8_t)n, 0}; E->midi(in, m, 3); }
static void cc(void *in, int c, int v) { uint8_t m[3] = {0xb0, (uint8_t)c, (uint8_t)v}; E->midi(in, m, 3); }
static void set(void *in, const char *k, const char *v) { E->set_param(in, k, v); }
static void setf(void *in, const char *k, float v) { char b[32]; snprintf(b, sizeof b, "%g", v); E->set_param(in, k, b); }
static float getf(void *in, const char *k) { char b[64]; return E->get_param(in, k, b, sizeof b) > 0 ? (float)atof(b) : -999.0f; }

struct Stats { double rms; float peak; int nan; };
static Stats render(void *in, int frames, int16_t *sink = 0) {
    static int16_t buf[256];
    Stats s = {0, 0, 0};
    long n = 0;
    while (frames > 0) {
        int c = frames < 128 ? frames : 128;
        E->render(in, buf, c);
        for (int i = 0; i < c; i++) {
            float v = buf[2 * i] / 32768.0f;
            if (v != v) s.nan++;
            s.rms += (double)v * v;
            if (fabsf(v) > s.peak) s.peak = fabsf(v);
            if (sink) sink[n] = buf[2 * i];
            n++;
        }
        if (sink) sink += 0;
        frames -= c;
    }
    s.rms = sqrt(s.rms / (n ? n : 1));
    return s;
}

static void reset_all(void *in) {
    cc(in, 123, 0);
    for (int i = 0; i < NPARAMS; i++) {
        if (PARAMS[i].momentary || PARAMS[i].popup_of >= 0) continue;
        float v = PARAMS[i].nopts ? roundf(PARAMS[i].def * (PARAMS[i].nopts - 1)) : PARAMS[i].min + (PARAMS[i].max - PARAMS[i].min) * PARAMS[i].def;
        setf(in, PARAMS[i].key, v);
    }
    render(in, 44100);   // let everything decay
}

// ---- WAV writer ----
static FILE *wav_open(const char *path, int rate) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    uint8_t h[44] = {0};
    memcpy(h, "RIFF", 4); memcpy(h + 8, "WAVEfmt ", 8); memcpy(h + 36, "data", 4);
    h[16] = 16; h[20] = 1; h[22] = 2;
    h[24] = rate & 255; h[25] = (rate >> 8) & 255; h[26] = (rate >> 16) & 255;
    int br = rate * 4; h[28] = br & 255; h[29] = (br >> 8) & 255; h[30] = (br >> 16) & 255; h[31] = (br >> 24) & 255;
    h[32] = 4; h[34] = 16;
    fwrite(h, 1, 44, f);
    return f;
}
static void wav_close(FILE *f) {
    long len = ftell(f);
    uint32_t d = (uint32_t)(len - 44), r = (uint32_t)(len - 8);
    fseek(f, 4, SEEK_SET); fwrite(&r, 4, 1, f);
    fseek(f, 40, SEEK_SET); fwrite(&d, 4, 1, f);
    fclose(f);
}
static void demo_render(void *in, FILE *f, int frames) {
    int16_t buf[256];
    while (frames > 0) {
        int c = frames < 128 ? frames : 128;
        E->render(in, buf, c);
        fwrite(buf, 4, c, f);
        frames -= c;
    }
}

int main(int argc, char **argv) {
    E = mpc_engine();
    void *in = E->create(0);
    CHECK(in != 0, "engine instance created");
    if (!in) return 1;

    // 1. silence at rest
    Stats s = render(in, 4096);
    CHECK(s.rms == 0.0, "silent before any note (rms %.5f)", s.rms);

    // 2. every model makes sound in every VCA mode, no NaN, and goes quiet after release
    const char *vca[3] = {"0", "1", "2"};
    for (int m = 0; m < 24; m++) {
        for (int v = 0; v < 3; v++) {
            reset_all(in);
            setf(in, "o1_engine", (float)m);
            set(in, "o2_on", "0");   // this checks one model at a time
            set(in, "vca_mode", vca[v]);
            setf(in, "decay", 30);
            note(in, 60, 100);
            Stats a = render(in, 22050);
            off(in, 60);
            Stats b = render(in, 44100 * 6);
            Stats c = render(in, 2048);
            CHECK(a.nan == 0 && b.nan == 0 && a.rms > 1e-3 && a.peak <= 1.0f && c.rms < 1e-3,
                  "model %2d (%-14s) vca %s: rms %.4f peak %.3f, after release %.5f", m, PARAMS[8].opts[m], v == 0 ? "ENV" : v == 1 ? "LPG" : "ON", a.rms, a.peak, c.rms);
        }
    }

    // 3. three oscillators, HP mode, drive, noise
    reset_all(in);
    set(in, "o2_on", "1"); set(in, "o3_on", "1");
    setf(in, "o2_semi", 7); setf(in, "o3_oct", -1);
    set(in, "f_mode", "1"); setf(in, "cutoff", 30); setf(in, "reso", 60); setf(in, "drive", 50); setf(in, "noise", 30);
    note(in, 48, 110);
    s = render(in, 22050);
    CHECK(s.nan == 0 && s.rms > 1e-3 && s.peak <= 1.0f, "3 oscs + HP + drive + noise: rms %.4f peak %.3f", s.rms, s.peak);
    off(in, 48);
    render(in, 44100 * 3);

    // 4. polyphony: 8 notes, then a 9th steals; VOICES=1 limits
    reset_all(in);
    setf(in, "attack", 0); setf(in, "sustain", 100);
    for (int i = 0; i < 8; i++) note(in, 48 + i * 2, 100);
    Stats poly = render(in, 8192);
    for (int i = 0; i < 8; i++) off(in, 48 + i * 2);
    render(in, 44100 * 3);
    note(in, 60, 100);
    Stats mono1 = render(in, 8192);
    off(in, 60);
    render(in, 44100 * 3);
    CHECK(poly.rms > mono1.rms * 1.5f, "8 voices louder than 1 (%.4f vs %.4f)", poly.rms, mono1.rms);
    for (int i = 0; i < 9; i++) note(in, 40 + i, 100);
    s = render(in, 4096);
    CHECK(s.nan == 0 && s.peak <= 1.0f, "9th note steals a voice without trouble (peak %.3f)", s.peak);
    for (int i = 0; i < 9; i++) off(in, 40 + i);
    render(in, 44100 * 3);

    // 5. MONO note stack: release the top note falls back to the held one, then silence when all released
    reset_all(in);
    set(in, "voice_mode", "1");
    note(in, 60, 100); render(in, 2048);
    note(in, 67, 100); render(in, 2048);
    off(in, 67);
    Stats held = render(in, 4096);
    off(in, 60);
    render(in, 44100 * 4);
    Stats quiet = render(in, 2048);
    CHECK(held.rms > 1e-3 && quiet.rms < 1e-3, "MONO: falls back to the held note (%.4f), then silent (%.5f)", held.rms, quiet.rms);

    // 6. LEGATO + glide: a second note while the first is held slides without a retrigger (no new attack dip)
    reset_all(in);
    set(in, "voice_mode", "2"); setf(in, "glide", 60); setf(in, "attack", 70);
    note(in, 48, 100); render(in, 44100);
    Stats before = render(in, 2048);
    note(in, 60, 100);
    Stats after = render(in, 2048);
    off(in, 60); off(in, 48);
    render(in, 44100 * 4);
    CHECK(after.rms > before.rms * 0.5f, "LEGATO: no retrigger dip on the second note (%.4f -> %.4f)", before.rms, after.rms);

    // 7. sustain pedal holds notes
    reset_all(in);
    setf(in, "decay", 10); setf(in, "sustain", 100);
    cc(in, 64, 127);
    note(in, 60, 100); render(in, 2048); off(in, 60);
    Stats pedal_on = render(in, 8192);
    cc(in, 64, 0);
    render(in, 44100 * 3);
    Stats pedal_off = render(in, 2048);
    CHECK(pedal_on.rms > 1e-3 && pedal_off.rms < 1e-3, "sustain pedal holds (%.4f) and releases (%.5f)", pedal_on.rms, pedal_off.rms);

    // 8. state round-trip
    reset_all(in);
    setf(in, "cutoff", 33); setf(in, "o2_engine", 17); set(in, "lfo_shape", "3");
    char st[8192];
    int n = E->get_param(in, "state", st, sizeof st);
    CHECK(n > 0 && n < 8000, "state is %d bytes", n);
    void *in2 = E->create(0);
    E->set_param(in2, "state", st);
    CHECK(fabsf(getf(in2, "cutoff") - 33) < 1e-3 && getf(in2, "o2_engine") == 17 && getf(in2, "lfo_shape") == 3,
          "state restores cutoff %.1f, o2_engine %.0f, lfo_shape %.0f", getf(in2, "cutoff"), getf(in2, "o2_engine"), getf(in2, "lfo_shape"));
    E->destroy(in2);

    // 9. CPU: worst case, 8 voices x 3 oscillators, heavy models, 10 s of audio
    reset_all(in);
    set(in, "o2_on", "1"); set(in, "o3_on", "1");
    setf(in, "o1_engine", 18); setf(in, "o2_engine", 21); setf(in, "o3_engine", 8);   // 6-op FM, wave terrain, swarm
    setf(in, "sustain", 100);
    for (int i = 0; i < 8; i++) note(in, 40 + i * 3, 100);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    s = render(in, 44100 * 10);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    printf("info 8 voices x 3 oscs (6-op FM / wave terrain / swarm): %.2f s to render 10 s = %.1f%% of one x86 core, %.0f us per 128-frame block\n",
           secs, secs * 10.0, secs / (10.0 * 44100 / 128) * 1e6);
    CHECK(s.nan == 0, "worst case: no NaN");
    for (int i = 0; i < 8; i++) off(in, 40 + i * 3);
    render(in, 44100 * 3);

    // 10. demo WAV
    if (argc > 1) {
        FILE *f = wav_open(argv[1], 44100);
        if (f) {
            reset_all(in);
            // part 1: Grind-style mono lead: VA osc, resonant LP, env -> filter, square LFO vibrato
            set(in, "voice_mode", "1"); setf(in, "glide", 35);
            setf(in, "o1_engine", 0); setf(in, "o1_harm", 45); setf(in, "o1_timbre", 60); setf(in, "o1_morph", 30);
            setf(in, "cutoff", 55); setf(in, "reso", 55); setf(in, "f_mod", 70); setf(in, "attack", 2); setf(in, "decay", 45); setf(in, "sustain", 35);
            int lead[] = {48, 48, 60, 55, 51, 48, 58, 60};
            for (int i = 0; i < 8; i++) { note(in, lead[i], 110); demo_render(in, f, 9000); off(in, lead[i]); demo_render(in, f, 2000); }
            demo_render(in, f, 22050);
            // part 2: poly pad: 3 oscs (VA + wavetable + string machine), slow attack, LFO on morph
            reset_all(in);
            set(in, "o2_on", "1"); set(in, "o3_on", "1");
            setf(in, "o1_engine", 0); setf(in, "o2_engine", 5); setf(in, "o3_engine", 22);
            setf(in, "o2_fine", 8); setf(in, "o3_fine", -6); setf(in, "o2_level", 60); setf(in, "o3_level", 70);
            setf(in, "o3_morph_mod", 40); setf(in, "lfo_rate", 20);
            setf(in, "attack", 60); setf(in, "decay", 70); setf(in, "sustain", 80); setf(in, "cutoff", 60); setf(in, "f_mod", 30);
            int pad[][4] = {{48, 55, 60, 64}, {45, 52, 57, 60}, {50, 57, 62, 65}, {43, 50, 55, 59}};
            for (int c = 0; c < 4; c++) {
                for (int k = 0; k < 4; k++) note(in, pad[c][k], 90);
                demo_render(in, f, 66150);
                for (int k = 0; k < 4; k++) off(in, pad[c][k]);
                demo_render(in, f, 22050);
            }
            demo_render(in, f, 66150);
            // part 3: LPG-mode percussion models played from "pads"
            reset_all(in);
            set(in, "vca_mode", "1"); setf(in, "lpg_decay", 40);
            int drums[] = {13, 14, 15, 15, 13, 14, 15, 15};
            for (int i = 0; i < 16; i++) {
                setf(in, "o1_engine", (float)drums[i % 8]);
                note(in, 60, i % 4 == 0 ? 120 : 80); demo_render(in, f, 2000); off(in, 60); demo_render(in, f, 3500);
            }
            demo_render(in, f, 44100);
            wav_close(f);
            printf("info demo written to %s\n", argv[1]);
        }
    }

    E->destroy(in);
    printf("%s\n", fails ? "FAILED" : "PASSED");
    return fails ? 1 : 0;
}
