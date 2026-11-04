// Millstone: an 8-voice hybrid synth for Akai MPC OS (Open MPC port kit).
// Each voice has three Mutable Instruments Plaits macro-oscillators, a 24 dB ladder filter (LP/HP),
// an ADS envelope, a VCA with ENV / LPG / ON modes; one global LFO. Modelled on the panel of a
// semi-modular hybrid desktop synth: VC MIX, OSC MOD (vibrato), glide, noise, VCF mod depth with an
// ENV/LFO source switch.
//
// Engine contract: wrapper/engine.h (44.1 kHz, interleaved int16 stereo, 1..128 frames per render()).
// Parameters: the generated params.h (keys, ranges, defaults) is the single source of truth.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>

extern "C" {
#include "engine.h"
}
#include "params.h"
#include "dsp_bits.h"
#include "stmlib/utils/buffer_allocator.h"
#include "plaits/dsp/voice.h"

namespace {

using namespace ms;

const int kVoices = 8;
const int kOscs = 3;
const int kChunk = 12;            // Plaits renders in 12-frame blocks on the hardware too
const int kOscRam = 16384;        // per-oscillator scratch for the Plaits engines
const int kNoteStack = 16;
const int kSilentChunks = 920;    // ~0.25 s of near-silence frees a voice

// Our model list (params.json order) -> Plaits engine index.
const int kEngineMap[24] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7};
// Engines that shape their own amplitude from TRIG (6-op FM gate, plucks, drums): TRIG is patched for them in every
// VCA mode. Chiptune (7) is left continuous (a chord engine) except in LPG mode, where TRIG clocks its arpeggiator.
inline bool self_enveloped(int plaits_engine) {
    return (plaits_engine >= 2 && plaits_engine <= 4) || plaits_engine >= 19;
}

// ---- parameter table lookups -----------------------------------------------------------------
int key_index(const char *key) {
    for (int i = 0; i < NPARAMS; i++)
        if (!strcmp(PARAMS[i].key, key)) return i;
    return -1;
}

enum PerOsc { O_ON, O_ENGINE, O_HARM, O_TIMBRE, O_MORPH, O_FM, O_TMOD, O_MMOD, O_OCT, O_SEMI, O_FINE, O_LEVEL, O_MIX, O_COUNT };
const char *const kOscSuffix[O_COUNT] = {"on", "engine", "harm", "timbre", "morph", "fm", "timbre_mod", "morph_mod",
                                         "oct", "semi", "fine", "level", "mix"};

enum Global {
    G_VOICES, G_VOICE_MODE, G_GLIDE, G_VIBRATO, G_NOISE, G_LPG_DECAY, G_LPG_COLOUR,
    G_CUTOFF, G_RESO, G_FMODE, G_FMOD, G_FSRC, G_KEYTRACK, G_DRIVE,
    G_ATTACK, G_DECAY, G_SUSTAIN, G_VCA_MODE, G_VOLUME, G_VEL_AMP, G_VEL_FILT,
    G_LFO_SHAPE, G_LFO_RATE, G_LFO_RETRIG, G_LFO_SYNC, G_LFO_DIV,
    G_WHEEL_LFO, G_AT_CUTOFF, G_AT_MORPH, G_AT_LFO, G_BEND_RANGE, G_COUNT
};
const char *const kGlobalKey[G_COUNT] = {
    "voices", "voice_mode", "glide", "vibrato", "noise", "lpg_decay", "lpg_colour",
    "cutoff", "reso", "f_mode", "f_mod", "f_src", "keytrack", "drive",
    "attack", "decay", "sustain", "vca_mode", "volume", "vel_amp", "vel_filt",
    "lfo_shape", "lfo_rate", "lfo_retrig", "lfo_sync", "lfo_div",
    "wheel_lfo", "at_cutoff", "at_morph", "at_lfo", "bend_range"};

struct Index {
    int g[G_COUNT];
    int o[kOscs][O_COUNT];
    bool ok;
    Index() : ok(true) {
        for (int i = 0; i < G_COUNT; i++) { g[i] = key_index(kGlobalKey[i]); if (g[i] < 0) ok = false; }
        for (int n = 0; n < kOscs; n++)
            for (int i = 0; i < O_COUNT; i++) {
                char k[32];
                snprintf(k, sizeof k, "o%d_%s", n + 1, kOscSuffix[i]);
                o[n][i] = key_index(k);
                if (o[n][i] < 0) ok = false;
            }
    }
};
const Index &IDX() { static Index idx; return idx; }

enum { VM_POLY, VM_MONO, VM_LEGATO };
enum { VCA_ENV, VCA_LPG, VCA_ON };
enum { FS_ENV, FS_LFO };

const float kDivBeats[8] = {16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f};

// ---- one oscillator slot: a Plaits voice with its own scratch memory ----------------------------
struct Osc {
    alignas(8) char ram[kOscRam];
    stmlib::BufferAllocator allocator;
    plaits::Voice voice;
    void init() {
        allocator.Init(ram, sizeof ram);
        voice.Init(&allocator);
    }
};

struct Voice {
    Osc osc[kOscs];
    Ladder filter;
    Env env;
    Noise noise;
    bool active = false;
    bool gate = false;        // key (or pedal) holding the note
    bool sustained = false;   // released under the sustain pedal
    int note = 60;
    float velocity = 1.0f;
    float aftertouch = 0.0f;  // poly aftertouch for this note
    float pitch_cur = 60.0f, pitch_target = 60.0f;
    int trig_low_chunks = 0;  // force Plaits TRIG low for a render or two, so a retrigger makes a new rising edge
    int silent_chunks = 0;
    uint32_t serial = 0;      // allocation order (oldest first)
    float gain_z = 0.0f;      // VCA smoothing (de-click for ON mode)
    plaits::Voice::Frame frames[kChunk];
};

struct Instance {
    float val[NPARAMS];
    Voice *voices[kVoices];
    Lfo lfo;
    float lfo_value = 0.0f;
    float bpm = 120.0f;
    float modwheel = 0.0f, chan_at = 0.0f, bend = 0.0f;
    bool pedal = false;
    int stack[kNoteStack];   // mono/legato note stack, most recent last
    int stack_vel[kNoteStack];
    int nstack = 0;
    uint32_t serial = 1;
    float last_pitch = 60.0f;
    float mix[kChunk];
    float out[kChunk];
    char state_buf[8192];

    float P(int g) const { return val[IDX().g[g]]; }
    float O(int osc, int p) const { return val[IDX().o[osc][p]]; }
};

// ---- defaults and state ----------------------------------------------------------------------
float param_default(int i) {
    const param_t &p = PARAMS[i];
    if (p.nopts) return (float)lroundf(p.def * (p.nopts - 1));
    return p.min + (p.max - p.min) * p.def;
}

float param_clamp(int i, float v) {
    const param_t &p = PARAMS[i];
    if (p.nopts) return (float)clampi((int)lroundf(v), 0, p.nopts - 1);
    return clampf(v, p.min, p.max);
}

void set_param_idx(Instance *in, int i, float v) {
    in->val[i] = param_clamp(i, v);
}

// ---- voice allocation -------------------------------------------------------------------------
int num_voices(const Instance *in) { return clampi((int)lroundf(in->P(G_VOICES)), 1, kVoices); }

void start_voice(Instance *in, Voice *v, int note, float vel, bool retrigger) {
    float glide = in->P(G_GLIDE) * 0.01f;
    v->pitch_target = (float)note;
    if (retrigger) {
        // glide from where this voice was, or from the last played pitch when it starts fresh
        v->pitch_cur = (glide > 0.0f) ? (v->active ? v->pitch_cur : in->last_pitch) : (float)note;
        v->env.gate_on();
        if (v->active) v->trig_low_chunks = 2;   // already sounding: a new strike needs TRIG to drop first
        if (!v->active) { v->filter.reset(); v->gain_z = 0.0f; }
    } else if (glide <= 0.0f) {
        v->pitch_cur = (float)note;
    }
    v->note = note;
    v->velocity = vel;
    v->aftertouch = 0.0f;
    v->gate = true;
    v->sustained = false;
    v->active = true;
    v->silent_chunks = 0;
    v->serial = in->serial++;
    in->last_pitch = (float)note;
    if (in->P(G_LFO_RETRIG) > 0.5f) in->lfo.retrigger();
}

void release_voice(Voice *v) {
    v->gate = false;
    v->sustained = false;
    v->env.gate_off();
}

Voice *find_voice_for_note(Instance *in, int note) {
    int n = num_voices(in);
    for (int i = 0; i < n; i++)
        if (in->voices[i]->active && in->voices[i]->gate && in->voices[i]->note == note) return in->voices[i];
    return 0;
}

Voice *alloc_voice(Instance *in) {
    int n = num_voices(in);
    Voice *best = 0;
    // 1. a free voice
    for (int i = 0; i < n; i++) if (!in->voices[i]->active) return in->voices[i];
    // 2. the quietest releasing voice
    float best_v = 1e9f;
    for (int i = 0; i < n; i++) {
        Voice *v = in->voices[i];
        if (!v->gate && v->env.value < best_v) { best_v = v->env.value; best = v; }
    }
    if (best) return best;
    // 3. the oldest
    uint32_t oldest = 0xffffffffu;
    for (int i = 0; i < n; i++)
        if (in->voices[i]->serial < oldest) { oldest = in->voices[i]->serial; best = in->voices[i]; }
    return best;
}

void stack_push(Instance *in, int note, int vel) {
    for (int i = 0; i < in->nstack; i++)
        if (in->stack[i] == note) {   // already held: move it to the top
            for (int j = i; j < in->nstack - 1; j++) { in->stack[j] = in->stack[j + 1]; in->stack_vel[j] = in->stack_vel[j + 1]; }
            in->nstack--;
            break;
        }
    if (in->nstack == kNoteStack) {
        for (int j = 0; j < kNoteStack - 1; j++) { in->stack[j] = in->stack[j + 1]; in->stack_vel[j] = in->stack_vel[j + 1]; }
        in->nstack--;
    }
    in->stack[in->nstack] = note;
    in->stack_vel[in->nstack] = vel;
    in->nstack++;
}

bool stack_remove(Instance *in, int note) {
    for (int i = 0; i < in->nstack; i++)
        if (in->stack[i] == note) {
            for (int j = i; j < in->nstack - 1; j++) { in->stack[j] = in->stack[j + 1]; in->stack_vel[j] = in->stack_vel[j + 1]; }
            in->nstack--;
            return true;
        }
    return false;
}

void note_on(Instance *in, int note, int vel) {
    float velocity = vel / 127.0f;
    int mode = (int)in->P(G_VOICE_MODE);
    if (mode == VM_POLY) {
        Voice *v = find_voice_for_note(in, note);
        if (!v) v = alloc_voice(in);
        start_voice(in, v, note, velocity, true);
    } else {
        bool was_held = in->nstack > 0;
        stack_push(in, note, vel);
        Voice *v = in->voices[0];
        bool retrig = (mode == VM_MONO) || !was_held || !v->active;
        start_voice(in, v, note, velocity, retrig);
    }
}

void note_off(Instance *in, int note) {
    int mode = (int)in->P(G_VOICE_MODE);
    if (mode == VM_POLY) {
        int n = num_voices(in);
        for (int i = 0; i < n; i++) {
            Voice *v = in->voices[i];
            if (v->active && v->gate && v->note == note) {
                if (in->pedal) v->sustained = true; else release_voice(v);
            }
        }
    } else {
        if (!stack_remove(in, note)) return;
        Voice *v = in->voices[0];
        if (in->nstack > 0) {
            // fall back to the most recently held note; MONO retriggers, LEGATO just slides
            int top = in->stack[in->nstack - 1];
            if (v->note == note) start_voice(in, v, top, in->stack_vel[in->nstack - 1] / 127.0f, mode == VM_MONO);
        } else if (v->active && v->gate) {
            if (in->pedal) v->sustained = true; else release_voice(v);
        }
    }
}

void pedal_up(Instance *in) {
    for (int i = 0; i < kVoices; i++) {
        Voice *v = in->voices[i];
        if (v->active && v->sustained) release_voice(v);
    }
}

void all_notes_off(Instance *in) {
    in->nstack = 0;
    for (int i = 0; i < kVoices; i++) if (in->voices[i]->active) release_voice(in->voices[i]);
}

// ---- rendering --------------------------------------------------------------------------------
inline float cutoff_hz(const Instance *in, const Voice *v, float env, float lfo) {
    float base_oct = 10.0f * in->P(G_CUTOFF) * 0.01f;               // 20 Hz .. 20.48 kHz
    float mod = in->P(G_FMOD) * 0.01f;
    float src = ((int)in->P(G_FSRC) == FS_ENV) ? env : lfo;
    float oct = base_oct + mod * src * 6.0f;
    oct += in->P(G_KEYTRACK) * 0.01f * (v->pitch_cur - 60.0f) / 12.0f;
    oct += in->P(G_VEL_FILT) * 0.01f * (v->velocity * 2.0f - 1.0f) * 3.0f;
    oct += in->P(G_AT_CUTOFF) * 0.01f * fmaxf(in->chan_at, v->aftertouch) * 4.0f;
    return 20.0f * powf(2.0f, clampf(oct, 0.0f, 10.0f));
}

void render_voice(Instance *in, Voice *v, int frames, float lfo) {
    const int vca_mode = (int)in->P(G_VCA_MODE);
    const float at = fmaxf(in->chan_at, v->aftertouch);
    const float bend = in->bend * in->P(G_BEND_RANGE);
    const float vib = in->P(G_VIBRATO) * 0.01f;
    const float vibrato = lfo * vib * vib * 12.0f;
    const float env_now = v->env.value;

    // glide: constant-time exponential approach, 5 ms .. 2 s
    float glide = in->P(G_GLIDE) * 0.01f;
    if (glide > 0.0f && v->pitch_cur != v->pitch_target) {
        float t = 0.005f * powf(400.0f, glide);
        float c = 1.0f - expf(-(float)frames / (t * kRate));
        v->pitch_cur += (v->pitch_target - v->pitch_cur) * c;
        if (fabsf(v->pitch_cur - v->pitch_target) < 0.001f) v->pitch_cur = v->pitch_target;
    } else if (glide <= 0.0f) {
        v->pitch_cur = v->pitch_target;
    }

    float *mix = in->mix;
    for (int i = 0; i < frames; i++) mix[i] = 0.0f;

    const bool gated = v->gate || v->sustained;
    const float trig = (gated && v->trig_low_chunks == 0) ? 1.0f : 0.0f;   // TRIG follows the gate (6-op FM needs it held)
    bool any_self_env = false;
    bool chip_clocked = false;
    for (int n = 0; n < kOscs; n++) {
        if (in->O(n, O_ON) < 0.5f) continue;
        float level = in->O(n, O_LEVEL) * 0.01f;
        if (level <= 0.0f) continue;
        int model = clampi((int)in->O(n, O_ENGINE), 0, 23);
        int engine = kEngineMap[model];
        bool trig_mode = (vca_mode == VCA_LPG) || self_enveloped(engine);
        if (self_enveloped(engine)) any_self_env = true;
        if (trig_mode && engine == 7) chip_clocked = true;   // chiptune clocked by TRIG: it never fades by itself

        plaits::Patch p;
        p.note = clampf(v->pitch_cur + bend + vibrato + in->O(n, O_OCT) * 12.0f + in->O(n, O_SEMI) + in->O(n, O_FINE) * 0.01f, 12.0f, 120.0f);
        p.harmonics = in->O(n, O_HARM) * 0.01f;
        p.timbre = in->O(n, O_TIMBRE) * 0.01f;
        p.morph = clampf(in->O(n, O_MORPH) * 0.01f + in->P(G_AT_MORPH) * 0.01f * at, 0.0f, 1.0f);
        p.frequency_modulation_amount = in->O(n, O_FM) * 0.01f;
        p.timbre_modulation_amount = in->O(n, O_TMOD) * 0.01f;
        p.morph_modulation_amount = in->O(n, O_MMOD) * 0.01f;
        p.engine = engine;
        p.decay = in->P(G_LPG_DECAY) * 0.01f;
        p.lpg_colour = in->P(G_LPG_COLOUR) * 0.01f;

        plaits::Modulations m;
        m.engine = 0.0f;
        m.note = 0.0f;
        m.frequency = env_now * 24.0f;                 // FM knob = envelope -> pitch, up to +-24 semitones
        m.harmonics = 0.0f;
        m.timbre = lfo;                                // TIMBRE CV / MORPH CV attenuators take the LFO
        m.morph = lfo;
        m.trigger = trig;
        m.level = v->velocity;
        m.frequency_patched = !trig_mode;              // in TRIG mode Plaits' own decay envelope sweeps the pitch
        m.timbre_patched = p.timbre_modulation_amount != 0.0f;
        m.morph_patched = p.morph_modulation_amount != 0.0f;
        m.trigger_patched = trig_mode;
        m.level_patched = trig_mode && vca_mode != VCA_LPG;   // LPG mode: TRIG strikes, LEVEL unpatched = accent fixed

        v->osc[n].voice.Render(p, m, v->frames, (size_t)frames);
        float wet = in->O(n, O_MIX) * 0.01f;
        float dry = 1.0f - wet;
        float g = level * level / 32768.0f;
        for (int i = 0; i < frames; i++)
            mix[i] += g * (dry * v->frames[i].out + wet * v->frames[i].aux);
    }
    if (v->trig_low_chunks > 0) v->trig_low_chunks--;

    // noise before the filter
    float noise = in->P(G_NOISE) * 0.01f;
    if (noise > 0.0f) {
        float g = noise * noise * 0.5f;
        for (int i = 0; i < frames; i++) mix[i] += g * v->noise.tick();
    }

    // filter, envelope, VCA
    float drive = in->P(G_DRIVE) * 0.01f;
    float pre = 1.0f + drive * 6.0f;
    float post = 1.0f / (1.0f + drive * 2.0f);
    v->filter.set(cutoff_hz(in, v, env_now, lfo), in->P(G_RESO) * 0.01f, (int)in->P(G_FMODE) == 1);
    v->env.set(in->P(G_ATTACK) * 0.01f, in->P(G_DECAY) * 0.01f, in->P(G_SUSTAIN) * 0.01f);
    float vel_amp = in->P(G_VEL_AMP) * 0.01f;
    float vel_gain = 1.0f - vel_amp + vel_amp * v->velocity;

    float target_gain;
    switch (vca_mode) {
        case VCA_LPG: target_gain = (gated || !chip_clocked) ? vel_gain : 0.0f; break;
        case VCA_ON:  target_gain = (gated || any_self_env) ? vel_gain : 0.0f; break;
        default:      target_gain = vel_gain; break;
    }
    float peak = 0.0f;
    for (int i = 0; i < frames; i++) {
        float e = v->env.tick();
        float y = v->filter.tick(mix[i] * pre) * post;
        v->gain_z += (target_gain - v->gain_z) * 0.01f;   // ~2 ms de-click
        float gain = vca_mode == VCA_ENV ? e * vel_gain : v->gain_z;
        y *= gain;
        in->out[i] += y;
        float a = fabsf(y);
        if (a > peak) peak = a;
    }

    // free the voice when it has nothing left to say
    bool env_done = (vca_mode == VCA_ENV) && v->env.idle() && !gated;
    if (peak < 1e-4f && !gated) v->silent_chunks++; else v->silent_chunks = 0;
    bool silent_done = v->silent_chunks > kSilentChunks && v->env.idle();
    bool on_done = (vca_mode != VCA_ENV) && !gated && v->gain_z < 1e-4f && (vca_mode == VCA_ON ? !any_self_env : chip_clocked);
    if (env_done || silent_done || on_done) {
        v->active = false;
        v->env.reset();
        v->gain_z = 0.0f;
    }
}

void render_chunk(Instance *in, int16_t *out_lr, int frames) {
    // LFO
    float hz;
    if (in->P(G_LFO_SYNC) > 0.5f) {
        int div = clampi((int)in->P(G_LFO_DIV), 0, 7);
        hz = clampf(in->bpm, 20.0f, 300.0f) / (60.0f * kDivBeats[div]);
    } else {
        hz = Lfo::rate_hz(in->P(G_LFO_RATE) * 0.01f);
    }
    float raw = in->lfo.step(hz, frames, (int)in->P(G_LFO_SHAPE));
    float w = in->P(G_WHEEL_LFO) * 0.01f;
    float scale = (1.0f - w) + w * in->modwheel + in->P(G_AT_LFO) * 0.01f * in->chan_at;
    in->lfo_value = raw * clampf(scale, 0.0f, 1.5f);

    for (int i = 0; i < frames; i++) in->out[i] = 0.0f;
    int n = num_voices(in);
    for (int i = 0; i < kVoices; i++) {
        Voice *v = in->voices[i];
        if (!v->active) continue;
        if (i >= n && !v->gate) { v->active = false; v->env.reset(); continue; }   // voice count lowered
        render_voice(in, v, frames, in->lfo_value);
    }

    float vol = in->P(G_VOLUME) * 0.01f;
    float g = vol * vol * 0.5f;
    for (int i = 0; i < frames; i++) {
        float y = fast_tanh(in->out[i] * g * 1.2f) * 0.95f;
        int16_t s = (int16_t)lrintf(y * 32767.0f);
        out_lr[2 * i] = s;
        out_lr[2 * i + 1] = s;
    }
}

// ---- engine interface -------------------------------------------------------------------------
void *ms_create(const char *) {
    if (!IDX().ok) return 0;   // params.json and this engine disagree: refuse to load rather than misbehave
    Instance *in = new (std::nothrow) Instance;
    if (!in) return 0;
    for (int i = 0; i < NPARAMS; i++) in->val[i] = param_default(i);
    for (int i = 0; i < kVoices; i++) {
        in->voices[i] = new (std::nothrow) Voice;
        if (!in->voices[i]) {
            for (int j = 0; j < i; j++) delete in->voices[j];
            delete in;
            return 0;
        }
        for (int n = 0; n < kOscs; n++) in->voices[i]->osc[n].init();
    }
    in->lfo.retrigger();
    return in;
}

void ms_destroy(void *inst) {
    Instance *in = (Instance *)inst;
    if (!in) return;
    for (int i = 0; i < kVoices; i++) delete in->voices[i];
    delete in;
}

void ms_midi(void *inst, const uint8_t *msg, int len) {
    Instance *in = (Instance *)inst;
    if (len < 2) return;
    int status = msg[0] & 0xf0;
    int d1 = msg[1] & 0x7f;
    int d2 = len > 2 ? msg[2] & 0x7f : 0;
    switch (status) {
        case 0x90:
            if (d2 > 0) note_on(in, d1, d2); else note_off(in, d1);
            break;
        case 0x80:
            note_off(in, d1);
            break;
        case 0xa0: {   // polyphonic aftertouch
            for (int i = 0; i < kVoices; i++)
                if (in->voices[i]->active && in->voices[i]->note == d1) in->voices[i]->aftertouch = d2 / 127.0f;
            break;
        }
        case 0xb0:
            if (d1 == 1) in->modwheel = d2 / 127.0f;
            else if (d1 == 64) {
                bool down = d2 >= 64;
                if (in->pedal && !down) { in->pedal = false; pedal_up(in); }
                in->pedal = down;
            } else if (d1 == 120 || d1 == 123) all_notes_off(in);
            else if (d1 == 121) { in->modwheel = 0.0f; in->chan_at = 0.0f; in->bend = 0.0f; }
            break;
        case 0xd0:
            in->chan_at = d1 / 127.0f;
            break;
        case 0xe0:
            in->bend = (((d2 << 7) | d1) - 8192) / 8192.0f;
            break;
        default: break;
    }
}

void ms_set_param(void *inst, const char *key, const char *val) {
    Instance *in = (Instance *)inst;
    if (!strcmp(key, "lfo_bpm")) { in->bpm = clampf((float)atof(val), 20.0f, 300.0f); return; }
    if (!strcmp(key, "transport")) return;
    if (!strcmp(key, "state")) {
        // "key=value\n" lines
        const char *p = val;
        while (*p) {
            const char *eq = strchr(p, '=');
            const char *nl = strchr(p, '\n');
            if (!nl) nl = p + strlen(p);
            if (eq && eq < nl) {
                char k[48];
                int kl = (int)(eq - p);
                if (kl > 0 && kl < (int)sizeof k) {
                    memcpy(k, p, kl);
                    k[kl] = 0;
                    int i = key_index(k);
                    if (i >= 0 && !PARAMS[i].momentary && PARAMS[i].popup_of < 0) set_param_idx(in, i, (float)atof(eq + 1));
                }
            }
            if (!*nl) break;
            p = nl + 1;
        }
        return;
    }
    int i = key_index(key);
    if (i < 0 || PARAMS[i].momentary) return;
    set_param_idx(in, i, (float)atof(val));
}

int ms_get_param(void *inst, const char *key, char *buf, int buf_len) {
    Instance *in = (Instance *)inst;
    if (!strcmp(key, "state")) {
        int pos = 0;
        for (int i = 0; i < NPARAMS; i++) {
            if (PARAMS[i].momentary || PARAMS[i].popup_of >= 0) continue;   // the wrapper owns popup flags
            int n = snprintf(buf + pos, buf_len - pos, "%s=%g\n", PARAMS[i].key, in->val[i]);
            if (n < 0 || pos + n >= buf_len) return 0;
            pos += n;
        }
        return pos;
    }
    int i = key_index(key);
    if (i < 0 || PARAMS[i].momentary) return 0;
    if (PARAMS[i].nopts || PARAMS[i].int_display) return snprintf(buf, buf_len, "%d", (int)lroundf(in->val[i]));
    return snprintf(buf, buf_len, "%g", in->val[i]);
}

void ms_render(void *inst, int16_t *out_lr, int frames) {
    Instance *in = (Instance *)inst;
    while (frames > 0) {
        int c = frames < kChunk ? frames : kChunk;
        render_chunk(in, out_lr, c);
        out_lr += 2 * c;
        frames -= c;
    }
}

const mpc_engine_t kEngine = {ms_create, ms_destroy, ms_midi, ms_set_param, ms_get_param, ms_render, 0, 0};

}  // namespace

extern "C" const mpc_engine_t *mpc_engine(void) { return &kEngine; }
