// VCA MODE / filter MOD SOURCE = TIDES n: AD opens the VCA and its tail outlives the key, AR follows its release, LOOP
// closes with the key, the filter follows a Tides output. Exits non-zero on a failure (tools/run_tests.sh runs it).
extern "C" {
#include "engine.h"
}
#include "params.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
// RMS (dB) in 50 ms windows of a note held HOLD s, then released, RENDER s in all.
static std::vector<double> run(const char *vca, const char *tmode, const char *fsrc, double hold, double total, const char *fmod = "0") {
    const mpc_engine_t *E = mpc_engine(); void *d = E->create(0);
    E->set_param(d, "voices", "1");
    E->set_param(d, "vca_mode", vca); E->set_param(d, "t_mode", tmode); E->set_param(d, "t_out", getenv("TOUT") ? getenv("TOUT") : "2");   // PHASE: every output the full shape
    E->set_param(d, "t_trig", "0"); E->set_param(d, "t_rate", "40"); E->set_param(d, "f_src", fsrc);
    E->set_param(d, "f_mod", fmod); E->set_param(d, "cutoff", "40"); E->set_param(d, "release", "5");
    uint8_t on[3] = {0x90, 60, 110}, off[3] = {0x80, 60, 0};
    E->midi(d, on, 3);
    std::vector<double> w; int16_t buf[256]; double acc = 0; int n = 0; bool released = false;
    for (long f = 0; f < (long)(total * 44100); f += 128) {
        if (!released && f >= hold * 44100) { E->midi(d, off, 3); released = true; }
        E->render(d, buf, 128);
        for (int i = 0; i < 256; i++) acc += (double)buf[i] * buf[i];
        n += 256;
        if (n >= 2205 * 2) { w.push_back(10 * log10(acc / n / (32768.0 * 32768.0) + 1e-14)); acc = 0; n = 0; }
    }
    E->destroy(d); return w;
}
static void show(const char *what, const std::vector<double> &w) {
    printf("%-34s", what); for (size_t i = 0; i < w.size(); i += 4) printf(" %5.0f", w[i]); printf("\n");
}
int main() {
    printf("%-34s", "50 ms windows, every 4th (dB):"); for (int i = 0; i < 40; i += 4) printf(" %4.1fs", i * 0.05); printf("\n");
    show("VCA ON (reference)", run("2", "0", "0", 1.0, 2.0));
    auto ad = run("3", "0", "0", 1.0, 2.0); show("VCA TIDES 1, Tides AD", ad);
    auto ar = run("3", "2", "0", 1.0, 2.0); show("VCA TIDES 1, Tides AR (release tail)", ar);
    auto lp = run("3", "1", "0", 1.0, 2.0); show("VCA TIDES 1, Tides LOOP", lp);
    // checks
    int fails = 0;
    double ad_peak = -200; for (auto x : ad) ad_peak = fmax(ad_peak, x);
    if (ad_peak < -40) { printf("FAIL: AD never opened the VCA\n"); fails++; }
    if (ad.back() > -90) { printf("FAIL: AD still sounding at the end (%.0f dB)\n", ad.back()); fails++; }
    if (lp[21] > -90 || lp.back() > -90) { printf("FAIL: LOOP did not close with the key (%.0f dB)\n", lp[21]); fails++; }
    if (ar[21] < ar.back() || ar[21] < -80) { printf("note: AR tail after release %.0f dB (window 21)\n", ar[21]); }
    double lo = 0, hi = -200; for (int i = 2; i < 19; i++) { lo = fmin(lo, lp[i]); hi = fmax(hi, lp[i]); }
    printf("LOOP swing while held: %.0f dB\n", hi - lo); if (hi - lo < 6) { printf("FAIL: LOOP barely moves the VCA\n"); fails++; }
    auto fa = run("2", "1", "3", 1.0, 1.0, "100"), fb = run("2", "1", "3", 1.0, 1.0, "0");   // filter follows a LOOP
    double sa = 0, sb = 0; for (int i = 2; i < 19; i++) { sa += fabs(fa[i] - fa[i - 1]); sb += fabs(fb[i] - fb[i - 1]); }
    printf("filter TIDES 1: level movement %.1f dB with MOD DEPTH 100 vs %.1f dB at 0\n", sa, sb);
    if (sa < sb + 3) { printf("FAIL: the filter does not follow Tides\n"); fails++; }
    printf(fails ? "FAILED %d\n" : "ALL OK\n", fails); return fails;
}
