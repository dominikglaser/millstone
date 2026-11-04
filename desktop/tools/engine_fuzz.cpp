// Developer check with no plugin around it: random patches and random MIDI, driven straight at the engine. Fails if a sample goes
// bad, or a note cannot be brought to an end. Build it with sanitizers using tools/sanitized_fuzz.sh. Usage: engine_fuzz [patches]
extern "C" {
#include "engine.h"
}
#include "params.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>

static uint32_t rng_state = 1;
static uint32_t next_random() { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }
static float unit() { return (float)(next_random() & 0xffffff) / 16777216.0f; }
static int below(int n) { return n > 0 ? (int)(next_random() % (uint32_t)n) : 0; }

int main(int argc, char **argv) {
    const int patches = argc > 1 ? atoi(argv[1]) : 150;
    const mpc_engine_t *E = mpc_engine();
    int failures = 0, longest = 0, longest_patch = 0, long_tails = 0;
    int16_t out[256];
    for (int seed = 1; seed <= patches; seed++) {
        rng_state = 0x9e3779b9u * (uint32_t)seed + 1;
        void *d = E->create(0);
        if (!d) { printf("FAIL patch %d: no engine\n", seed); return 1; }
        char v[32];
        for (int i = 0; i < NPARAMS; i++) {
            const param_t &p = PARAMS[i];
            if (p.momentary || p.popup_of >= 0) continue;
            if (p.nopts) snprintf(v, sizeof v, "%d", below(p.nopts));
            else if (p.int_display) snprintf(v, sizeof v, "%d", (int)p.min + below((int)(p.max - p.min) + 1));
            else snprintf(v, sizeof v, "%g", p.min + (p.max - p.min) * unit());
            E->set_param(d, p.key, v);
        }
        E->set_param(d, "volume", "60");
        if (const char *only = getenv("FUZZ_SET")) {   // for tracking a failure down: FUZZ_SET=reso=0,noise=0
            std::string list(only);
            size_t at = 0;
            while (at < list.size()) {
                const size_t comma = std::min(list.find(',', at), list.size()), eq = list.find('=', at);
                E->set_param(d, list.substr(at, eq - at).c_str(), list.substr(eq + 1, comma - eq - 1).c_str());
                at = comma + 1;
            }
        }
        double ppq = 0.0;
        bool bad = false;
        const auto play = [&](int frames) {
            while (frames > 0) {
                const int n = 1 + below(128);
                const int c = n < frames ? n : frames;
                if (E->song_pos) E->song_pos(d, ppq, 120.0, seed % 3 != 0 ? 1 : 0, c);
                ppq += c * 2.0 / 44100.0;
                E->render(d, out, c);
                for (int i = 0; i < 2 * c; i++) if (out[i] == -32768) bad = true;
                frames -= c;
            }
        };
        for (int step = 0; step < 40; step++) {
            uint8_t m[3] = {0, (uint8_t)below(128), (uint8_t)below(128)};
            switch (below(10)) {
                case 0: case 1: case 2: case 3: m[0] = 0x90; break;
                case 4: case 5: m[0] = 0x80; break;
                case 6: m[0] = 0xb0; m[1] = below(2) ? 1 : 64; break;
                case 7: m[0] = 0xe0; break;
                case 8: m[0] = 0xd0; break;
                default: m[0] = 0xa0; break;
            }
            E->midi(d, m, m[0] == 0xd0 ? 2 : 3);
            if (below(8) == 0) { snprintf(v, sizeof v, "%d", below(3)); E->set_param(d, "voice_mode", v); }
            if (below(8) == 0) { snprintf(v, sizeof v, "%d", 1 + below(8)); E->set_param(d, "voices", v); }
            play(below(3000));
        }
        // Everything off, and short tails: whatever this patch is, it has to come to an end.
        E->set_param(d, "release", "10");
        E->set_param(d, "m_release", "10");
        E->set_param(d, "lpg_decay", "20");
        E->set_param(d, "e_damp", "20");
        E->set_param(d, "e_space", "0");
        uint8_t pedal[3] = {0xb0, 64, 0};
        E->midi(d, pedal, 3);
        for (int note = 0; note < 128; note++) { uint8_t off[3] = {0x80, (uint8_t)note, 0}; E->midi(d, off, 3); }
        play(44100 * 10);
        // A struck model with its own decay turned up rings like a bell. That is fine; endless is not. The engine fades anything
        // still ringing 30 s after key up, so by 40 s everything has to be silent.
        int top = 0;
        int seconds = 10;
        for (; seconds < 40; seconds++) {
            top = 0;
            for (int i = 0; i < 44100; i += 128) {
                E->render(d, out, 128);
                for (int k = 0; k < 256; k++) if (abs(out[k]) > top) top = abs(out[k]);
            }
            if (top <= 32) break;
        }
        if (seconds > longest) { longest = seconds; longest_patch = seed; }
        if (seconds > 20) long_tails++;
        if (bad) { printf("FAIL patch %d: a sample left the valid range\n", seed); failures++; }
        if (top > 32) {
            printf("FAIL patch %d: still sounding 40 s after every note was released, peak %d\n", seed, top);
            failures++;
            if (getenv("FUZZ_VERBOSE")) {
                for (const char *key : {"vca_mode", "reso", "cutoff", "f_mode", "drive", "noise", "o1_type", "o1_on", "o1_engine", "o1_level", "o2_on", "o2_engine", "o2_level", "o3_on", "o3_engine", "o3_level"}) { char b[32]; if (E->get_param(d, key, b, sizeof b) > 0) printf(" %s=%s", key, b); }
                printf("\n cords:");
                for (int c = 1; c <= 12; c++) { char k[16], a[16], b[16], e[16]; snprintf(k, 16, "c%d_src", c); E->get_param(d, k, a, 16); snprintf(k, 16, "c%d_dst", c); E->get_param(d, k, b, 16); snprintf(k, 16, "c%d_amt", c); E->get_param(d, k, e, 16); printf(" %s>%s(%.0f)", a, b, atof(e)); }
                printf("\n");
            }
        }
        E->destroy(d);
    }
    printf("longest tail: %d s (patch %d), %d over 20 s\n", longest, longest_patch, long_tails);
    printf("%d random patches\n%s (%d failed)\n", patches, failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
