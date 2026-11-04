// Millstone: an 8-voice hybrid synth for Akai MPC OS (Open MPC port kit).
// Each voice has three Mutable Instruments Plaits macro-oscillators, a 24 dB ladder filter (LP/HP),
// a VCA ADSR and a modulation ADSR, a VCA with ENV / LPG / ON modes; one global LFO. Modelled on the panel of a
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
#ifdef MILLSTONE_DESKTOP   // the desktop plugin only, until the MPC build can be tested
#include "stmlib/utils/gate_flags.h"
#include "tides2/poly_slope_generator.h"
#include "workout_clock.h"
#include "elements/dsp/part.h"
#endif

namespace {

using namespace ms;

const int kVoices = 8;
const int kOscs = 3;
const int kCords = 12;            // patch bay cords: each is a source, a destination and an amount
const int kChunk = 12;            // Plaits renders in 12-frame blocks on the hardware too
const int kOscRam = 16384;        // per-oscillator scratch for the Plaits engines
const int kNoteStack = 16;
const int kSilentChunks = 920;    // ~0.25 s of near-silence frees a voice
// String and Modal at full MORPH have no decay at all, and with their trigger patched nothing else would stop them.
const int kMaxRingChunks = 110250;    // 30 s after key up, a voice starts to fade...
const int kRingFadeChunks = 11025;    // ...over 3 s, and is then freed

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

enum PerOsc { O_ON, O_ENGINE, O_HARM, O_TIMBRE, O_MORPH, O_FM, O_TMOD, O_MMOD, O_OCT, O_SEMI, O_FINE, O_LEVEL, O_MIX, O_CV_SRC, O_AMP, O_COUNT };
const char *const kOscSuffix[O_COUNT] = {"on", "engine", "harm", "timbre", "morph", "fm", "timbre_mod", "morph_mod",
                                         "oct", "semi", "fine", "level", "mix", "cv_src", "amp"}
;
enum OscAmp { AMP_AUTO, AMP_ENV, AMP_LPG };

enum Global {
    G_VOICES, G_VOICE_MODE, G_GLIDE, G_VIBRATO, G_NOISE, G_LPG_DECAY, G_LPG_COLOUR,
    G_CUTOFF, G_RESO, G_FMODE, G_FMOD, G_FSRC, G_KEYTRACK, G_DRIVE,
    G_ATTACK, G_DECAY, G_SUSTAIN, G_RELEASE, G_M_ATTACK, G_M_DECAY, G_M_SUSTAIN, G_M_RELEASE, G_VCA_MODE, G_VOLUME, G_VEL_AMP, G_VEL_FILT,
    G_LFO_SHAPE, G_LFO_RATE, G_LFO_RETRIG, G_LFO_SYNC, G_LFO_DIV, G_LFO_LOCK,
    G_WHEEL_LFO, G_AT_CUTOFF, G_AT_MORPH, G_AT_LFO, G_BEND_RANGE,
    G_T_MODE, G_T_OUT, G_T_TRIG, G_T_DIV, G_T_RATE, G_T_SHAPE, G_T_SLOPE, G_T_SMOOTH, G_T_SHIFT, G_CK_SRC, G_CK_BPM,
    G_O1_TYPE, G_E_MODEL, G_E_CONTOUR, G_E_BOW, G_E_BOW_T, G_E_BLOW, G_E_FLOW, G_E_BLOW_T, G_E_STRIKE, G_E_MALLET, G_E_STRIKE_T, G_E_GEOM, G_E_BRIGHT, G_E_DAMP, G_E_POS, G_E_SPACE, G_COUNT
};
// Patch bay. These match the option order of cN_src and cN_dst in params.json.
enum CordSource { S_OFF, S_LFO, S_VCA_ENV, S_MOD_ENV, S_VELOCITY, S_AFTERTOUCH, S_WHEEL, S_KEY, S_GATE, S_NOISE, S_OSC1, S_OSC2, S_OSC3, S_TIDES1, S_TIDES2, S_TIDES3, S_TIDES4, S_CLOCK1, S_CLOCK8 = S_CLOCK1 + 7 };
enum CordDest { D_OFF, D_PITCH, D_CUTOFF, D_RESO, D_VCA, D_NOISE, D_OSC1, D_OSC_COUNT = 5, D_T_RATE = D_OSC1 + 3 * D_OSC_COUNT, D_T_SHAPE, D_T_SLOPE, D_T_SMOOTH, D_T_SHIFT, D_T_TRIG, D_LFO_RESET, D_ENV_TRIG, D_MOD_TRIG, D_O1_TRIG, D_O2_TRIG, D_O3_TRIG, D_E_GEOM, D_E_BRIGHT, D_E_DAMP, D_E_POS, D_E_SPACE, D_E_BOW, D_E_BLOW, D_E_STRIKE, D_E_STRIKE_IN, D_E_BLOW_IN, D_COUNT };
const float kTriggerThreshold = 0.25f;   // a trigger input fires as its cords rise past this
enum OscDest { OD_PITCH, OD_HARM, OD_TIMBRE, OD_MORPH, OD_LEVEL };
const float kCordPitch = 24.0f;    // semitones at full amount and full signal
const float kCordCutoff = 6.0f;    // octaves, the same reach as MOD DEPTH
const float kLevelSlew = 0.01f;    // about 2 ms: a gate into a level input snaps without clicking. Audio-rate cords skip this

const char *const kGlobalKey[G_COUNT] = {
    "voices", "voice_mode", "glide", "vibrato", "noise", "lpg_decay", "lpg_colour",
    "cutoff", "reso", "f_mode", "f_mod", "f_src", "keytrack", "drive",
    "attack", "decay", "sustain", "release", "m_attack", "m_decay", "m_sustain", "m_release", "vca_mode", "volume", "vel_amp", "vel_filt",
    "lfo_shape", "lfo_rate", "lfo_retrig", "lfo_sync", "lfo_div", "lfo_lock",
    "wheel_lfo", "at_cutoff", "at_morph", "at_lfo", "bend_range",
    "t_mode", "t_out", "t_trig", "t_div", "t_rate", "t_shape", "t_slope", "t_smooth", "t_shift", "ck_src", "ck_bpm",
    "o1_type", "e_model", "e_contour", "e_bow", "e_bow_t", "e_blow", "e_flow", "e_blow_t", "e_strike", "e_mallet", "e_strike_t", "e_geom", "e_bright", "e_damp", "e_pos", "e_space"};

const char *const kClockSuffix[11] = {"div", "shape", "width", "swing", "prob", "phase", "level", "offset", "steps", "fill", "rot"};
enum ClockField { CK_DIV, CK_SHAPE, CK_WIDTH, CK_SWING, CK_PROB, CK_PHASE, CK_LEVEL, CK_OFFSET, CK_STEPS, CK_FILL, CK_ROT };
const float kClockRates[19] = {1.0f / 32, 1.0f / 16, 1.0f / 8, 1.0f / 7, 1.0f / 6, 1.0f / 5, 1.0f / 4, 1.0f / 3, 1.0f / 2, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16};

struct Index {
    int g[G_COUNT];
    int o[kOscs][O_COUNT];
    int cord[kCords][3];   // source, destination, amount
    int clock[8][11];      // per channel, in the order of kClockSuffix
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
        for (int c = 0; c < kCords; c++)
            for (int i = 0; i < 3; i++) {
                char k[32];
                snprintf(k, sizeof k, "c%d_%s", c + 1, i == 0 ? "src" : i == 1 ? "dst" : "amt");
                cord[c][i] = key_index(k);
                if (cord[c][i] < 0) ok = false;
            }
        for (int c = 0; c < 8; c++)
            for (int i = 0; i < 11; i++) {
                char k[32];
                snprintf(k, sizeof k, "k%d_%s", c + 1, kClockSuffix[i]);
                clock[c][i] = key_index(k);
                if (clock[c][i] < 0) ok = false;
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

#ifdef MILLSTONE_DESKTOP
// Elements on OSC 1. It runs at its own 32 kHz, because its tuning and delays depend on that, and is resampled to ours.
struct ElementsVoice {
    enum { kBlock = 16 };
    elements::Part part;
    uint16_t reverb[32768];
    float history[4];       // the last four samples, for interpolating
    float position;         // between history[1] and history[2]
    float exciter_open;     // audio patched into the exciter stops with the key, or the resonator would ring forever
    float main[kBlock], aux[kBlock], blow_in[kBlock], strike_in[kBlock];
    int block_at;           // next unread sample of the block
    float aux_history[4];
    uint32_t seed;
    void init(uint32_t voice_seed) {   // also how a voice is cleared: Part declares a Panic() but never implements it
        seed = voice_seed;
        memset(reverb, 0, sizeof reverb);
        part.Init(reverb);
        part.Seed(&seed, 1);           // every module differs slightly by serial number; here, every voice does
        part.set_easter_egg(false);
        for (int i = 0; i < 4; i++) history[i] = aux_history[i] = 0.0f;
        position = 0.0f;
        exciter_open = 0.0f;
        block_at = kBlock;
    }
};
#endif

struct Voice {
    Osc osc[kOscs];
    Ladder filter;
    Env env;    // VCA
    Env menv;   // modulation: filter, oscillator FM, and TIMBRE/MORPH CV when selected
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
    int released_chunks = 0;
    float vca_cord = 0.0f;        // what the patch bay is doing to these levels, smoothed
    float level_cord[kOscs] = {0.0f, 0.0f, 0.0f};
    float noise_cord = 0.0f;
    bool cords_settled = false;   // a new note starts at the patched level, rather than sliding to it
    float osc_gate = 0.0f;        // per-oscillator routing: the de-clicked key, for a free-running oscillator in VCA ON
    int input_quiet_chunks = 0;   // released, and nothing going into the filter: a self-oscillating filter must not ring forever
    uint32_t serial = 0;      // allocation order (oldest first)
    float gain_z = 0.0f;      // VCA smoothing (de-click for ON mode)
    plaits::Voice::Frame frames[kChunk];
    float osc_audio[kOscs][kChunk];   // what each oscillator played this chunk, for cords that patch it
    float noise_audio[kChunk];
    float osc_last[kOscs];            // its last sample, for destinations that only change once per chunk
    float noise_last;
    float tides_out[4];               // Tides' four outputs, normalised: 0..1 or -1..1
    float tides_free_phase;           // FREE: what fires the one-shot modes
    bool trigger_high[7];             // trigger inputs, for spotting a rising edge
    int osc_trigger_low[kOscs];       // force an oscillator's TRIG low for a render or two, so a cord can strike it
#ifdef MILLSTONE_DESKTOP
    tides::PolySlopeGenerator tides;
    stmlib::GateFlags tides_gate;
    ElementsVoice *elements;
    bool elements_stale;              // a new note took this voice: last note's resonance and reverb must not come back
#endif
};

struct Instance {
    float val[NPARAMS];
    Voice *voices[kVoices];
    Lfo lfo;
    float lfo_value = 0.0f;
    float lfo_raw = 0.0f;      // before the wheel and pressure scale it: the patch bay's LFO jack
    bool tides_on = false;     // something is patched from Tides, so it needs running
    float bpm = 120.0f;
    float clock_out[8] = {};       // the shared clock's outputs
    double clock_position = 0.0;   // in beats
    bool clock_on = false;
#ifdef MILLSTONE_DESKTOP
    WorkoutClock clock;
#endif
    float tides_clock = 0.0f;      // CLOCK: phase of the divided clock, 0..1
    float tides_clock_hz = 2.0f;
    double song_ppq = 0.0;     // host song position in beats, at the latest block start
    bool song_fresh = false;   // ...and the LFO has not lined up with it yet
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

void all_notes_off(Instance *in);

void set_param_idx(Instance *in, int i, float v) {
    const float next = param_clamp(i, v);
    // POLY and MONO/LEGATO track held keys differently, so a key held across the change could never be released.
    if (i == IDX().g[G_VOICE_MODE] && next != in->val[i]) all_notes_off(in);
    in->val[i] = next;
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
        v->menv.gate_on();
        if (!v->active) {
            for (int n = 0; n < kOscs; n++) v->osc_last[n] = 0.0f;
            v->noise_last = 0.0f;
            v->cords_settled = false;
#ifdef MILLSTONE_DESKTOP
            v->elements_stale = true;
#endif
        }
        if (v->active) v->trig_low_chunks = 2;   // already sounding: a new strike needs TRIG to drop first
        if (!v->active) { v->filter.reset(); v->gain_z = 0.0f; }
        v->velocity = vel;                       // a legato note continues the phrase at its level
        if (in->P(G_LFO_RETRIG) > 0.5f) in->lfo.retrigger();
    } else if (glide <= 0.0f) {
        v->pitch_cur = (float)note;
    }
    v->note = note;
    v->aftertouch = 0.0f;
    v->gate = true;
    v->sustained = false;
    v->active = true;
    v->silent_chunks = 0;
    v->input_quiet_chunks = 0;
    v->released_chunks = 0;
    v->serial = in->serial++;
    in->last_pitch = (float)note;
}

void release_voice(Voice *v) {
    v->gate = false;
    v->sustained = false;
    v->env.gate_off();
    v->menv.gate_off();
}

Voice *find_voice_for_note(Instance *in, int note) {
    for (int i = 0; i < kVoices; i++)   // all of them: VOICES may have been lowered while this note was held
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
        for (int i = 0; i < kVoices; i++) {   // all of them: VOICES may have been lowered while this note was held
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

void all_sound_off(Instance *in) {   // MIDI CC120: silence now, without the release
    in->nstack = 0;
    for (int i = 0; i < kVoices; i++) {
        Voice *v = in->voices[i];
        v->active = v->gate = v->sustained = false;
        v->env.reset(); v->menv.reset();
        v->gain_z = 0.0f;
    }
}

// ---- rendering --------------------------------------------------------------------------------
inline float cutoff_octaves(const Instance *in, const Voice *v, float env, float lfo) {
    float base_oct = 10.0f * in->P(G_CUTOFF) * 0.01f;               // 20 Hz .. 20.48 kHz
    float mod = in->P(G_FMOD) * 0.01f;
    float src = ((int)in->P(G_FSRC) == FS_ENV) ? env : lfo;
    float oct = base_oct + mod * src * 6.0f;
    oct += in->P(G_KEYTRACK) * 0.01f * (v->pitch_cur - 60.0f) / 12.0f;
    oct += in->P(G_VEL_FILT) * 0.01f * (v->velocity * 2.0f - 1.0f) * 3.0f;
    oct += in->P(G_AT_CUTOFF) * 0.01f * fmaxf(in->chan_at, v->aftertouch) * 4.0f;
    return oct;
}

inline float octaves_to_hz(float oct) { return 20.0f * powf(2.0f, clampf(oct, 0.0f, 10.0f)); }

// One voice's view of the patch bay for one chunk.
struct CordState {
    float amount[D_COUNT] = {};            // summed: everything that only needs to change once per chunk
    struct { int source; int dest; float amount; } fast[kCords];   // audio into the filter or VCA: applied every sample
    int fast_count = 0;
    struct { int source; int dest; float amount; } exciter[kCords];   // audio into Elements' exciter
    int exciter_count = 0;
    bool osc_used[kOscs] = {};
    bool noise_used = false;
    bool fast_filter = false;
    float osc_dest(int osc, int what) const { return amount[D_OSC1 + osc * D_OSC_COUNT + what]; }
};

inline bool audio_source(int s) { return s >= S_NOISE && s <= S_OSC3; }

CordState read_cords(const Instance *in, const Voice *v, bool gated, float at) {
    CordState st;
    const Index &ix = IDX();
    for (int c = 0; c < kCords; c++) {
        const int source = (int)in->val[ix.cord[c][0]], dest = (int)in->val[ix.cord[c][1]];
        const float amount = in->val[ix.cord[c][2]] * 0.01f;
        if (source == S_OFF || dest == D_OFF || amount == 0.0f) continue;
        if (source == S_NOISE) st.noise_used = true;
        if (source >= S_OSC1 && source <= S_OSC3) st.osc_used[source - S_OSC1] = true;
        if (audio_source(source) && (dest == D_E_STRIKE_IN || dest == D_E_BLOW_IN)) {
            st.exciter[st.exciter_count].source = source;
            st.exciter[st.exciter_count].dest = dest;
            st.exciter[st.exciter_count].amount = amount;
            st.exciter_count++;
            continue;
        }
        if (audio_source(source) && (dest == D_CUTOFF || dest == D_RESO || dest == D_VCA)) {
            st.fast[st.fast_count].source = source;
            st.fast[st.fast_count].dest = dest;
            st.fast[st.fast_count].amount = amount;
            st.fast_count++;
            if (dest != D_VCA) st.fast_filter = true;
            continue;
        }
        float value = 0.0f;
        switch (source) {
            case S_LFO: value = in->lfo_raw; break;
            case S_VCA_ENV: value = v->env.value; break;
            case S_MOD_ENV: value = v->menv.value; break;
            case S_VELOCITY: value = v->velocity; break;
            case S_AFTERTOUCH: value = at; break;
            case S_WHEEL: value = in->modwheel; break;
            case S_KEY: value = (v->pitch_cur - 60.0f) / 60.0f; break;
            case S_GATE: value = gated ? 1.0f : 0.0f; break;
            case S_NOISE: value = v->noise_last; break;
            case S_TIDES1: case S_TIDES2: case S_TIDES3: case S_TIDES4: value = v->tides_out[source - S_TIDES1]; break;
            case S_CLOCK1: case S_CLOCK1 + 1: case S_CLOCK1 + 2: case S_CLOCK1 + 3: case S_CLOCK1 + 4: case S_CLOCK1 + 5: case S_CLOCK1 + 6: case S_CLOCK8: value = in->clock_out[source - S_CLOCK1]; break;
            default: value = v->osc_last[source - S_OSC1]; break;   // stepped like a CV input on the real module
        }
        st.amount[dest] += value * amount;
    }
    return st;
}

#ifdef MILLSTONE_DESKTOP
enum { TT_NOTE, TT_FREE, TT_CLOCK, TT_PATCH };   // PATCH: only T TRIG fires it

// One Tides per voice, run once per chunk: it has no idea of a sample rate, only a phase increment per call, and this
// costs about what reading an LFO does. Its outputs are eurorack volts, so they are scaled down to the patch bay's range.
void tides_step(Instance *in, Voice *v, int index, const CordState *cords, bool gated, bool triggered, bool held, int frames) {
    const int trigger = (int)in->P(G_T_TRIG), mode = (int)in->P(G_T_MODE);
    const auto cord = [&](int dest) { return cords ? cords->amount[dest] : 0.0f; };
    float hz = Lfo::rate_hz(in->P(G_T_RATE) * 0.01f) * powf(2.0f, cord(D_T_RATE) * kCordCutoff);
    bool gate = false;
    float ramp = 0.0f;
    const float *ramp_in = 0;
    if (trigger == TT_NOTE) {
        gate = gated;
    } else if (trigger == TT_FREE) {
        // A hair apart per voice, so a held chord drifts rather than moving as one.
        hz *= 1.0f + 0.0023f * (float)index;
        v->tides_free_phase += hz * frames / kRate;
        v->tides_free_phase -= floorf(v->tides_free_phase);
        gate = mode != tides::RAMP_MODE_LOOPING && v->tides_free_phase < 0.5f;
    } else if (trigger == TT_CLOCK) {
        hz = in->tides_clock_hz;
        if (mode == tides::RAMP_MODE_LOOPING) { ramp = in->tides_clock; ramp_in = &ramp; }
        else gate = in->tides_clock < 0.5f;
    }
    // A trigger always fires, even with the gate already held open by a note, which has no rising edge of its own.
    v->tides_gate = triggered ? (stmlib::GateFlags)(stmlib::GATE_FLAG_HIGH | stmlib::GATE_FLAG_RISING)
                              : stmlib::ExtractGateFlags(v->tides_gate, gate || held);   // held: AR stays open as long as the cord is high
    tides::PolySlopeGenerator::OutputSample out;
    v->tides.Render((tides::RampMode)mode, (tides::OutputMode)(int)in->P(G_T_OUT), tides::RANGE_CONTROL, hz * frames / kRate,
                    clampf(in->P(G_T_SLOPE) * 0.01f + cord(D_T_SLOPE), 0.0f, 1.0f),
                    clampf(in->P(G_T_SHAPE) * 0.01f + cord(D_T_SHAPE), 0.0f, 1.0f),
                    clampf(in->P(G_T_SMOOTH) * 0.01f + cord(D_T_SMOOTH), 0.0f, 1.0f),
                    clampf(in->P(G_T_SHIFT) * 0.01f + cord(D_T_SHIFT), 0.0f, 1.0f),
                    &v->tides_gate, ramp_in, &out, 1);
    const float scale = mode == tides::RAMP_MODE_LOOPING ? 1.0f / 5.0f : 1.0f / 8.0f;
    for (int i = 0; i < 4; i++) v->tides_out[i] = clampf(out.channel[i] * scale, -1.0f, 1.0f);
}

bool source_patched(const Instance *in, int first, int last) {
    const Index &ix = IDX();
    for (int c = 0; c < kCords; c++) {
        const int source = (int)in->val[ix.cord[c][0]];
        if (source >= first && source <= last && (int)in->val[ix.cord[c][1]] != D_OFF) return true;
    }
    return false;
}

// The shared clock. While the song plays it follows the song position; otherwise it runs at the tempo.
void clock_step(Instance *in, int frames) {
    const bool free_running = (int)in->P(G_CK_SRC) == 1;
    const float bpm = free_running ? in->P(G_CK_BPM) : clampf(in->bpm, 20.0f, 300.0f);
    if (!free_running && in->song_fresh) in->clock_position = in->song_ppq;
    else in->clock_position += (double)(bpm / 60.0f * (float)frames / kRate);
    in->clock_on = source_patched(in, S_CLOCK1, S_CLOCK8);
    if (!in->clock_on) return;
    const Index &ix = IDX();
    ClockChannelSettings settings[WorkoutClock::kChannels];
    for (int c = 0; c < WorkoutClock::kChannels; c++) {
        ClockChannelSettings &s = settings[c];
        const int *k = ix.clock[c];
        s.rate = kClockRates[clampi((int)in->val[k[CK_DIV]], 0, 18)];
        s.shape = (int)in->val[k[CK_SHAPE]];
        s.width = in->val[k[CK_WIDTH]] * 0.01f;
        s.swing = in->val[k[CK_SWING]] * 0.01f;
        s.probability = in->val[k[CK_PROB]] * 0.01f;
        s.phase = in->val[k[CK_PHASE]] * 0.01f;
        s.level = in->val[k[CK_LEVEL]] * 0.01f;
        s.offset = in->val[k[CK_OFFSET]] * 0.01f;
        s.steps = (int)in->val[k[CK_STEPS]];
        s.fill = (int)in->val[k[CK_FILL]];
        s.rotate = (int)in->val[k[CK_ROT]];
    }
    in->clock.process(settings, in->clock_position, 60.0f / bpm);
    for (int c = 0; c < WorkoutClock::kChannels; c++) in->clock_out[c] = in->clock.out[c];
}
#endif

#ifdef MILLSTONE_DESKTOP
inline float catmull(const float *h, float t) {
    const float t2 = t * t, t3 = t2 * t;
    return 0.5f * (2.0f * h[1] + (-h[0] + h[2]) * t + (2.0f * h[0] - 5.0f * h[1] + 4.0f * h[2] - h[3]) * t2 + (-h[0] + 3.0f * h[1] - 3.0f * h[2] + h[3]) * t3);
}

// Plays Elements into the voice's frames, in place of a Plaits oscillator, so the rest of the voice treats it the same.
void render_elements(Instance *in, Voice *v, const CordState &cords, float note, float strength, bool gate, bool held, int frames) {
    ElementsVoice &e = *v->elements;
    if (v->elements_stale) {
        e.init(e.seed);
        v->elements_stale = false;
    }
    elements::Patch &patch = *e.part.mutable_patch();
    const auto knob = [&](int g, int dest) { return clampf(in->P(g) * 0.01f + cords.amount[dest], 0.0f, 1.0f); };
    patch.exciter_envelope_shape = in->P(G_E_CONTOUR) * 0.01f;
    patch.exciter_bow_level = knob(G_E_BOW, D_E_BOW);
    patch.exciter_bow_timbre = in->P(G_E_BOW_T) * 0.01f;
    patch.exciter_blow_level = knob(G_E_BLOW, D_E_BLOW);
    patch.exciter_blow_meta = in->P(G_E_FLOW) * 0.01f;
    patch.exciter_blow_timbre = in->P(G_E_BLOW_T) * 0.01f;
    patch.exciter_strike_level = knob(G_E_STRIKE, D_E_STRIKE);
    patch.exciter_strike_meta = in->P(G_E_MALLET) * 0.01f;
    patch.exciter_strike_timbre = in->P(G_E_STRIKE_T) * 0.01f;
    patch.resonator_geometry = knob(G_E_GEOM, D_E_GEOM);
    patch.resonator_brightness = knob(G_E_BRIGHT, D_E_BRIGHT);
    patch.resonator_damping = knob(G_E_DAMP, D_E_DAMP);
    patch.resonator_position = knob(G_E_POS, D_E_POS);
    patch.space = knob(G_E_SPACE, D_E_SPACE);
    e.part.set_resonator_model((elements::ResonatorModel)clampi((int)in->P(G_E_MODEL), 0, 2));

    elements::PerformanceState state;
    state.gate = gate;
    state.note = note;
    state.modulation = 0.0f;
    state.strength = strength;

    const float ratio = 32000.0f / kRate;
    for (int i = 0; i < frames; i++) {
        e.position += ratio;
        while (e.position >= 1.0f) {
            e.position -= 1.0f;
            if (e.block_at >= ElementsVoice::kBlock) {
                // Anything patched into the exciter: the latest chunk of audio, stretched over this block.
                for (int k = 0; k < ElementsVoice::kBlock; k++) {
                    const int at = k * frames / ElementsVoice::kBlock;
                    float blow = cords.amount[D_E_BLOW_IN], strike = cords.amount[D_E_STRIKE_IN];
                    for (int c = 0; c < cords.exciter_count; c++) {
                        const int source = cords.exciter[c].source;
                        const float signal = (source == S_NOISE ? v->noise_audio[at] : v->osc_audio[source - S_OSC1][at]) * cords.exciter[c].amount;
                        if (cords.exciter[c].dest == D_E_BLOW_IN) blow += signal; else strike += signal;
                    }
                    e.exciter_open += ((held ? 1.0f : 0.0f) - e.exciter_open) * 0.02f;
                    e.blow_in[k] = blow * e.exciter_open;
                    e.strike_in[k] = strike * e.exciter_open;
                }
                e.part.Process(state, e.blow_in, e.strike_in, e.main, e.aux, ElementsVoice::kBlock);
                e.block_at = 0;
            }
            float main = e.main[e.block_at], aux = e.aux[e.block_at];
            e.block_at++;
            if (!(fabsf(main) < 8.0f) || !(fabsf(aux) < 8.0f)) { main = aux = 0.0f; v->elements_stale = true; }   // never let a blown-up resonator through: cleared on the next chunk
            for (int h = 0; h < 3; h++) { e.history[h] = e.history[h + 1]; e.aux_history[h] = e.aux_history[h + 1]; }
            e.history[3] = main;
            e.aux_history[3] = aux;
        }
        const float m = catmull(e.history, e.position), a = catmull(e.aux_history, e.position);
        // Its two outputs differ only in their stereo spread and reverb, so together they make the mono voice.
        v->frames[i].out = (int16_t)clampf((m + a) * 0.5f * 26000.0f, -32767.0f, 32767.0f);
        v->frames[i].aux = (int16_t)clampf(a * 26000.0f, -32767.0f, 32767.0f);
    }
}
#endif

void render_voice(Instance *in, Voice *v, int index, int frames, float lfo) {
    const int vca_mode = (int)in->P(G_VCA_MODE);
    const float at = fmaxf(in->chan_at, v->aftertouch);
    const float bend = in->bend * in->P(G_BEND_RANGE);
    const float vib = in->P(G_VIBRATO) * 0.01f;
    const float vibrato = lfo * vib * vib * 12.0f;
    const float env_now = v->menv.value;

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
    const CordState cords = read_cords(in, v, gated, at);

    // An oscillator set to ENV or LPG takes its amp on its own. The VCA sits after the mix, so its envelope would also cut an LPG
    // oscillator's ring: when any oscillator is set, the envelope moves in front of the mix, for those routed to it.
    bool per_osc = false;
    for (int n = 0; n < kOscs; n++) if (in->O(n, O_ON) >= 0.5f && (int)in->O(n, O_AMP) != AMP_AUTO) per_osc = true;
    float env_curve[kChunk], gate_curve[kChunk];
    if (per_osc) {
        v->env.set(in->P(G_ATTACK) * 0.01f, in->P(G_DECAY) * 0.01f, in->P(G_SUSTAIN) * 0.01f, in->P(G_RELEASE) * 0.01f);
        for (int i = 0; i < frames; i++) {
            env_curve[i] = v->env.tick();
            v->osc_gate += ((gated ? 1.0f : 0.0f) - v->osc_gate) * 0.01f;
            gate_curve[i] = v->osc_gate;
        }
    }
    // Trigger inputs fire once as their cords rise.
    bool fired[7];
    for (int t = 0; t < 7; t++) {
        const bool high = cords.amount[D_T_TRIG + t] > kTriggerThreshold;
        fired[t] = high && !v->trigger_high[t];
        v->trigger_high[t] = high;
    }
    if (fired[D_LFO_RESET - D_T_TRIG]) in->lfo.retrigger();
    if (gated && fired[D_ENV_TRIG - D_T_TRIG]) v->env.gate_on();
    if (gated && fired[D_MOD_TRIG - D_T_TRIG]) v->menv.gate_on();
    for (int n = 0; n < kOscs; n++) if (fired[D_O1_TRIG - D_T_TRIG + n]) v->osc_trigger_low[n] = 2;
#ifdef MILLSTONE_DESKTOP
    if (in->tides_on) tides_step(in, v, index, &cords, gated, fired[0], v->trigger_high[0], frames);
#else
    (void)index;
#endif
    if (!v->cords_settled) {
        v->vca_cord = cords.amount[D_VCA];
        v->noise_cord = cords.amount[D_NOISE];
        for (int n = 0; n < kOscs; n++) v->level_cord[n] = cords.osc_dest(n, OD_LEVEL);
        v->cords_settled = true;
    }
    const bool noise_moving = v->noise_cord != cords.amount[D_NOISE];
    const float noise_level = clampf(in->P(G_NOISE) * 0.01f + (noise_moving ? fmaxf(v->noise_cord, cords.amount[D_NOISE]) : cords.amount[D_NOISE]), 0.0f, 1.0f);
    bool any_self_env = false;
    bool any_free_running = noise_level > 0.0f;   // noise, or an oscillator that would sound forever without a gate
    bool chip_clocked = false;
    for (int n = 0; n < kOscs; n++) {
        if (cords.osc_used[n]) {   // a switched-off oscillator patches silence
            for (int i = 0; i < frames; i++) v->osc_audio[n][i] = 0.0f;
            v->osc_last[n] = 0.0f;
        }
        if (in->O(n, O_ON) < 0.5f) continue;
        const float level_knob = in->O(n, O_LEVEL) * 0.01f, level_target = cords.osc_dest(n, OD_LEVEL);
        if (fabsf(v->level_cord[n] - level_target) < 1e-4f) v->level_cord[n] = level_target;
        const bool level_moving = v->level_cord[n] != level_target;
        const float level = clampf(level_knob + (level_moving ? fmaxf(v->level_cord[n], level_target) : level_target), 0.0f, 1.0f);
        if (level <= 0.0f && !cords.osc_used[n]) continue;   // LEVEL 0 with a cord: heard only through what it modulates
        int model = clampi((int)in->O(n, O_ENGINE), 0, 23);
        int engine = kEngineMap[model];
        const int amp = (int)in->O(n, O_AMP);
        bool trig_mode = amp == AMP_ENV ? false : amp == AMP_LPG ? true : (vca_mode == VCA_LPG) || self_enveloped(engine);
#ifdef MILLSTONE_DESKTOP
        const bool use_elements = n == 0 && (int)in->P(G_O1_TYPE) == 1;
#else
        const bool use_elements = false;
#endif
        const bool ends_by_itself = use_elements || (amp == AMP_ENV ? false : amp == AMP_LPG || self_enveloped(engine));
        if (level > 0.0f) { if (ends_by_itself) any_self_env = true; else any_free_running = true; }
        if (trig_mode && engine == 7) chip_clocked = true;   // chiptune clocked by TRIG: it never fades by itself

        plaits::Patch p;
        p.note = clampf(v->pitch_cur + bend + vibrato + in->O(n, O_OCT) * 12.0f + in->O(n, O_SEMI) + in->O(n, O_FINE) * 0.01f
                        + (cords.amount[D_PITCH] + cords.osc_dest(n, OD_PITCH)) * kCordPitch, 12.0f, 120.0f);
        p.harmonics = clampf(in->O(n, O_HARM) * 0.01f + cords.osc_dest(n, OD_HARM), 0.0f, 1.0f);
        p.timbre = clampf(in->O(n, O_TIMBRE) * 0.01f + cords.osc_dest(n, OD_TIMBRE), 0.0f, 1.0f);
        p.morph = clampf(in->O(n, O_MORPH) * 0.01f + in->P(G_AT_MORPH) * 0.01f * at + cords.osc_dest(n, OD_MORPH), 0.0f, 1.0f);
        p.frequency_modulation_amount = in->O(n, O_FM) * 0.01f;
        p.timbre_modulation_amount = in->O(n, O_TMOD) * 0.01f;
        p.morph_modulation_amount = in->O(n, O_MMOD) * 0.01f;
        p.engine = engine;
        p.decay = in->P(G_LPG_DECAY) * 0.01f;
        p.lpg_colour = in->P(G_LPG_COLOUR) * 0.01f;

        plaits::Modulations m;
        m.engine = 0.0f;
        m.note = 0.0f;
        m.frequency = env_now * 24.0f;                 // FM knob = mod envelope -> pitch, up to +-24 semitones
        m.harmonics = 0.0f;
        const float cv = (int)in->O(n, O_CV_SRC) == 1 ? env_now : lfo;   // TIMBRE CV / MORPH CV attenuators take this
        m.timbre = cv;
        m.morph = cv;
        m.trigger = v->osc_trigger_low[n] > 0 ? 0.0f : trig;
        m.level = v->velocity;
        m.frequency_patched = !trig_mode;              // in TRIG mode Plaits' own decay envelope sweeps the pitch
        m.timbre_patched = p.timbre_modulation_amount != 0.0f;
        m.morph_patched = p.morph_modulation_amount != 0.0f;
        m.trigger_patched = trig_mode;
        m.level_patched = trig_mode && vca_mode != VCA_LPG && amp != AMP_LPG;   // LPG mode: TRIG strikes, LEVEL unpatched = accent fixed

#ifdef MILLSTONE_DESKTOP
        if (use_elements)
            render_elements(in, v, cords, p.note, v->velocity, gated && v->trig_low_chunks == 0 && v->osc_trigger_low[n] == 0, gated, frames);
        else
#endif
        v->osc[n].voice.Render(p, m, v->frames, (size_t)frames);
        if (cords.osc_used[n]) {
            for (int i = 0; i < frames; i++) v->osc_audio[n][i] = clampf(v->frames[i].out * (1.5f / 32768.0f), -1.0f, 1.0f);
            v->osc_last[n] = v->osc_audio[n][frames - 1];
        }
        float wet = in->O(n, O_MIX) * 0.01f;
        float dry = 1.0f - wet;
        float g = level * level / 32768.0f;
        float gains[kChunk];
        for (int i = 0; i < frames; i++) {
            if (level_moving) {
                v->level_cord[n] += (level_target - v->level_cord[n]) * kLevelSlew;
                const float l = clampf(level_knob + v->level_cord[n], 0.0f, 1.0f);
                gains[i] = l * l / 32768.0f;
            } else gains[i] = g;
        }
        if (per_osc) {
            // ENV: the amp envelope. LPG: nothing here, its gate has done the work. AUTO follows VCA MODE.
            const bool through_env = amp == AMP_ENV || (amp == AMP_AUTO && vca_mode == VCA_ENV);
            const bool through_gate = amp == AMP_AUTO && vca_mode == VCA_ON && !ends_by_itself;
            for (int i = 0; i < frames; i++) {
                const float shape = through_env ? env_curve[i] : through_gate ? gate_curve[i] : 1.0f;
                mix[i] += gains[i] * shape * (dry * v->frames[i].out + wet * v->frames[i].aux);
            }
        } else
        for (int i = 0; i < frames; i++)
            mix[i] += gains[i] * (dry * v->frames[i].out + wet * v->frames[i].aux);
    }
    if (v->trig_low_chunks > 0) v->trig_low_chunks--;
    for (int n = 0; n < kOscs; n++) if (v->osc_trigger_low[n] > 0) v->osc_trigger_low[n]--;

    // noise before the filter
    if (cords.noise_used) {
        for (int i = 0; i < frames; i++) v->noise_audio[i] = v->noise.tick();
        v->noise_last = v->noise_audio[frames - 1];
    }
    if (noise_level > 0.0f) {
        float g = noise_level * noise_level * 0.5f;
        const float noise_knob = in->P(G_NOISE) * 0.01f, noise_target = cords.amount[D_NOISE];
        if (vca_mode == VCA_LPG && !per_osc) g *= v->env.value;   // Plaits' gate does not reach the noise, so the envelope shapes it
        for (int i = 0; i < frames; i++) {
            if (noise_moving) {
                v->noise_cord += (noise_target - v->noise_cord) * kLevelSlew;
                if (fabsf(v->noise_cord - noise_target) < 1e-4f) v->noise_cord = noise_target;
                const float l = clampf(noise_knob + v->noise_cord, 0.0f, 1.0f);
                g = l * l * 0.5f;
                if (vca_mode == VCA_LPG && !per_osc) g *= v->env.value;
            }
            mix[i] += g * (per_osc ? (vca_mode == VCA_ON ? gate_curve[i] : env_curve[i]) : 1.0f) * (cords.noise_used ? v->noise_audio[i] : v->noise.tick());
        }
    }

    // filter, envelope, VCA
    float drive = in->P(G_DRIVE) * 0.01f;
    float pre = 1.0f + drive * 6.0f;
    float post = 1.0f / (1.0f + drive * 2.0f);
    const float base_oct = cutoff_octaves(in, v, env_now, lfo) + cords.amount[D_CUTOFF] * kCordCutoff;
    const float base_reso = in->P(G_RESO) * 0.01f + cords.amount[D_RESO];
    const int highpass = (int)in->P(G_FMODE) == 1;
    v->filter.set(octaves_to_hz(base_oct), base_reso, highpass);
    const float base_vca = cords.amount[D_VCA];
    v->env.set(in->P(G_ATTACK) * 0.01f, in->P(G_DECAY) * 0.01f, in->P(G_SUSTAIN) * 0.01f, in->P(G_RELEASE) * 0.01f);
    v->menv.set(in->P(G_M_ATTACK) * 0.01f, in->P(G_M_DECAY) * 0.01f, in->P(G_M_SUSTAIN) * 0.01f, in->P(G_M_RELEASE) * 0.01f);
    float vel_amp = in->P(G_VEL_AMP) * 0.01f;
    float vel_gain = 1.0f - vel_amp + vel_amp * v->velocity;

    float input_peak = 0.0f;
    for (int i = 0; i < frames; i++) { const float a = fabsf(mix[i]); if (a > input_peak) input_peak = a; }
    if (gated || input_peak >= 1e-4f) v->input_quiet_chunks = 0; else v->input_quiet_chunks++;
    const bool input_gone = v->input_quiet_chunks > kSilentChunks;
    v->released_chunks = gated ? 0 : v->released_chunks + 1;
    const float ring_gain = v->released_chunks <= kMaxRingChunks ? 1.0f : clampf(1.0f - (float)(v->released_chunks - kMaxRingChunks) / kRingFadeChunks, 0.0f, 1.0f);
    // Stay open after key up only if everything sounding ends by itself; anything else would hang.
    const bool self_ending = any_self_env && !any_free_running;
    float target_gain;
    switch (vca_mode) {
        case VCA_LPG: target_gain = ((gated || !chip_clocked) && !input_gone) ? vel_gain : 0.0f; break;
        case VCA_ON:  target_gain = ((gated || self_ending) && !input_gone) ? vel_gain : 0.0f; break;
        default:      target_gain = vel_gain; break;
    }
    if (per_osc) target_gain = (gated || !input_gone) ? vel_gain : 0.0f;   // the VCA only finishes what is left of the voice
    float peak = 0.0f;
    for (int i = 0; i < frames; i++) {
        float e = per_osc ? env_curve[i] : v->env.tick();
        v->menv.tick();
        v->vca_cord += (base_vca - v->vca_cord) * kLevelSlew;
        if (fabsf(v->vca_cord - base_vca) < 1e-5f) v->vca_cord = base_vca;
        float cutoff_mod = 0.0f, reso_mod = 0.0f, vca_mod = v->vca_cord;
        for (int c = 0; c < cords.fast_count; c++) {   // audio-rate cords: the sidebands come from doing this every sample
            const int source = cords.fast[c].source;
            const float signal = (source == S_NOISE ? v->noise_audio[i] : v->osc_audio[source - S_OSC1][i]) * cords.fast[c].amount;
            if (cords.fast[c].dest == D_CUTOFF) cutoff_mod += signal;
            else if (cords.fast[c].dest == D_RESO) reso_mod += signal;
            else vca_mod += signal;
        }
        if (cords.fast_filter) v->filter.set(octaves_to_hz(base_oct + cutoff_mod * kCordCutoff), base_reso + reso_mod, highpass);
        float y = v->filter.tick(mix[i] * pre) * post;
        v->gain_z += (target_gain - v->gain_z) * 0.01f;   // ~2 ms de-click
        float gain = vca_mode == VCA_ENV && !per_osc ? e * vel_gain : v->gain_z;
        y *= gain * ring_gain * clampf(1.0f + vca_mod, 0.0f, 2.0f);
        in->out[i] += y;
        float a = fabsf(y);
        if (a > peak) peak = a;
    }

    // free the voice when it has nothing left to say
    bool env_done = (vca_mode == VCA_ENV) && !per_osc && v->env.idle() && !gated;
    if (peak < 1e-4f && !gated) v->silent_chunks++; else v->silent_chunks = 0;
    bool silent_done = v->silent_chunks > kSilentChunks && (per_osc || vca_mode != VCA_ENV || v->env.idle());
    bool on_done = (per_osc || vca_mode != VCA_ENV) && !gated && v->gain_z < 1e-4f && (input_gone || (!per_osc && (vca_mode == VCA_ON ? !self_ending : chip_clocked)));
    const bool ring_done = v->released_chunks > kMaxRingChunks + kRingFadeChunks;
    if (env_done || silent_done || on_done || ring_done) {
        v->active = false;
        v->env.reset(); v->menv.reset();
        v->gain_z = 0.0f;
    }
}

void render_chunk(Instance *in, int16_t *out_lr, int frames) {
#ifdef MILLSTONE_DESKTOP
    in->tides_on = source_patched(in, S_TIDES1, S_TIDES4);
    clock_step(in, frames);
    if (in->tides_on && (int)in->P(G_T_TRIG) == TT_CLOCK) {
        static const float beats[12] = {32, 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f};
        const float length = beats[clampi((int)in->P(G_T_DIV), 0, 11)];
        in->tides_clock_hz = clampf(in->bpm, 20.0f, 300.0f) / (60.0f * length);
        if (in->song_fresh) {   // lined up with the song while it plays; at the tempo while it is stopped
            const double cycles = in->song_ppq / length;
            in->tides_clock = (float)(cycles - floor(cycles));
        } else {
            in->tides_clock += in->tides_clock_hz * frames / kRate;
            in->tides_clock -= floorf(in->tides_clock);
        }
    }
#endif
    // LFO
    float hz;
    if (in->P(G_LFO_SYNC) > 0.5f) {
        int div = clampi((int)in->P(G_LFO_DIV), 0, 7);
        hz = clampf(in->bpm, 20.0f, 300.0f) / (60.0f * kDivBeats[div]);
        // BAR: take the phase from the song position, so the LFO lands the same way on every playback. A stopped
        // transport sends no position, so it free-runs at the tempo between plays rather than freezing.
        if (in->song_fresh && (int)in->P(G_LFO_LOCK) == 1) {
            const double cycles = in->song_ppq / kDivBeats[div];
            in->lfo.set_phase((float)(cycles - floor(cycles)));
        }
    } else {
        hz = Lfo::rate_hz(in->P(G_LFO_RATE) * 0.01f);
    }
    in->song_fresh = false;
    float raw = in->lfo.step(hz, frames, (int)in->P(G_LFO_SHAPE));
    float w = in->P(G_WHEEL_LFO) * 0.01f;
    float pressure = in->chan_at;   // the LFO is shared, so the hardest-pressed key speaks for it
    for (int i = 0; i < kVoices; i++)
        if (in->voices[i]->active) pressure = fmaxf(pressure, in->voices[i]->aftertouch);
    float scale = (1.0f - w) + w * in->modwheel + in->P(G_AT_LFO) * 0.01f * pressure;
    in->lfo_raw = raw;
    in->lfo_value = raw * clampf(scale, 0.0f, 1.5f);

    for (int i = 0; i < frames; i++) in->out[i] = 0.0f;
    int n = num_voices(in);
    for (int i = 0; i < kVoices; i++) {
        Voice *v = in->voices[i];
        if (!v->active) {
#ifdef MILLSTONE_DESKTOP
            // FREE and CLOCK keep time between notes, so a new note lands wherever the cycle has got to.
            if (in->tides_on && ((int)in->P(G_T_TRIG) == TT_FREE || (int)in->P(G_T_TRIG) == TT_CLOCK)) tides_step(in, v, i, 0, false, false, false, frames);
#endif
            continue;
        }
        if (i >= n && !v->gate) { v->active = false; v->env.reset(); v->menv.reset(); continue; }   // voice count lowered
        render_voice(in, v, i, frames, in->lfo_value);
    }

    float vol = in->P(G_VOLUME) * 0.01f;
    float g = vol * vol * 0.5f;
    for (int i = 0; i < frames; i++) {
        float y = fast_tanh(in->out[i] * g * 2.0f) * 0.95f;
        int16_t s = (int16_t)lrintf(y * 32767.0f);
        out_lr[2 * i] = s;
        out_lr[2 * i + 1] = s;
    }
}

// ---- engine interface -------------------------------------------------------------------------
void *ms_create(const char *) {
    if (!IDX().ok) return 0;   // params.json and this engine disagree: refuse to load rather than misbehave
    // The () zeroes every member first. Plaits leaves some state to chance (ChiptuneEngine::Init skips its last
    // oscillator, whose phase could then read as NaN and hang the audio thread), so nothing may start as garbage.
    Instance *in = new (std::nothrow) Instance();
    if (!in) return 0;
    for (int i = 0; i < NPARAMS; i++) in->val[i] = param_default(i);
    for (int i = 0; i < kVoices; i++) {
        in->voices[i] = new (std::nothrow) Voice();
        if (!in->voices[i]) {
            for (int j = 0; j < i; j++) delete in->voices[j];
            delete in;
            return 0;
        }
        for (int n = 0; n < kOscs; n++) in->voices[i]->osc[n].init();
#ifdef MILLSTONE_DESKTOP
        in->voices[i]->tides.Init();
        in->voices[i]->tides_gate = stmlib::GATE_FLAG_LOW;
        in->voices[i]->elements = new (std::nothrow) ElementsVoice();
        if (!in->voices[i]->elements) {
            for (int j = 0; j <= i; j++) { delete in->voices[j]->elements; delete in->voices[j]; }
            delete in;
            return 0;
        }
        in->voices[i]->elements->init(0x9e3779b9u * (uint32_t)(i + 1));
#endif
    }
    in->lfo.retrigger();
    return in;
}

void ms_destroy(void *inst) {
    Instance *in = (Instance *)inst;
    if (!in) return;
    for (int i = 0; i < kVoices; i++) {
#ifdef MILLSTONE_DESKTOP
        delete in->voices[i]->elements;
#endif
        delete in->voices[i];
    }
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
            } else if (d1 == 120) all_sound_off(in);
            else if (d1 == 123) all_notes_off(in);
            else if (d1 == 121) {
                in->modwheel = 0.0f; in->chan_at = 0.0f; in->bend = 0.0f;
                in->pedal = false; pedal_up(in);
                for (int i = 0; i < kVoices; i++) in->voices[i]->aftertouch = 0.0f;
            }
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

void ms_song_pos(void *inst, double ppq, double, int playing, int) {
    Instance *in = (Instance *)inst;
    if (!playing || ppq < 0.0) return;
    in->song_ppq = ppq;
    in->song_fresh = true;
}

const mpc_engine_t kEngine = {ms_create, ms_destroy, ms_midi, ms_set_param, ms_get_param, ms_render, 0, ms_song_pos};

}  // namespace

extern "C" const mpc_engine_t *mpc_engine(void) { return &kEngine; }
