#ifndef SYNTHRAVE_INSTRUMENTS_EXT_H
#define SYNTHRAVE_INSTRUMENTS_EXT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Shared constants for block-based synth stubs. */
typedef struct {
    float sample_rate;
    float block_duration;
} SynthBlockConfig;

/** Laser-style FX sweep. */
typedef struct {
    float base_frequency;
    float target_frequency;
    float phase;
    float sweep_pos;
    float resonance;
} LaserSynthState;

void laser_synth_init(LaserSynthState *state, float start_freq, float end_freq, float resonance);
void laser_synth_process(LaserSynthState *state,
                         const SynthBlockConfig *cfg,
                         float *out,
                         size_t frames);

/** Choir pad built from multiple detuned sines. */
typedef struct {
    float root_frequency;
    float detune_cents[3];
    float phases[4];
    float envelope;
} ChoirSynthState;

void choir_synth_init(ChoirSynthState *state, float root_frequency);
void choir_synth_process(ChoirSynthState *state,
                         const SynthBlockConfig *cfg,
                         float softness,
                         float *out,
                         size_t frames);

/** Analog-style lead with mild portamento between targets. */
typedef struct {
    float current_frequency;
    float target_frequency;
    float glide_rate;
    float phase;
} AnalogLeadState;

void analog_lead_init(AnalogLeadState *state, float start_frequency, float glide_rate);
void analog_lead_set_target(AnalogLeadState *state, float target_frequency);
void analog_lead_process(AnalogLeadState *state,
                         const SynthBlockConfig *cfg,
                         float *out,
                         size_t frames);

/** SID-inspired bass with stepped volume envelope. */
typedef struct {
    float frequency;
    float phase;
    float step_duration;
    float time_in_step;
    int step_index;
} SidBassState;

void sid_bass_init(SidBassState *state, float frequency, float step_duration_ms);
void sid_bass_process(SidBassState *state,
                      const SynthBlockConfig *cfg,
                      float *out,
                      size_t frames);

/** Chip-arp generator that rotates up to four notes. */
typedef struct {
    float notes_hz[4];
    size_t note_count;
    size_t current_note;
    float phase;
    float tick_duration;
    float tick_time;
} ChipArpState;

void chip_arp_init(ChipArpState *state, const float *notes_hz, size_t note_count, float tick_ms);
void chip_arp_process(ChipArpState *state,
                      const SynthBlockConfig *cfg,
                      float *out,
                      size_t frames);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHRAVE_INSTRUMENTS_EXT_H */

/** Legacy percussion and melodic instrument states. */
typedef struct {
    float phase;
    float sweep_pos;
    float body_phase;
    float click_env;
} KickState;

void kick_state_init(KickState *state);
void kick_process(KickState *state,
                  const SynthBlockConfig *cfg,
                  float start_freq,
                  float end_freq,
                  float duration_s,
                  float *out,
                  size_t frames);

typedef struct {
    float noise_seed;
    float body_phase;
    float env_noise;
    float env_body;
} SnareState;

void snare_state_init(SnareState *state);
void snare_process(SnareState *state,
                   const SynthBlockConfig *cfg,
                   float body_freq,
                   float duration_s,
                   float *out,
                   size_t frames);

typedef struct {
    float noise_seed;
    float metallic_phase;
    float env;
} HatState;

void hat_state_init(HatState *state);
void hat_process(HatState *state,
                 const SynthBlockConfig *cfg,
                 float *out,
                 size_t frames);

typedef struct {
    float phase_main;
    float phase_sub;
    float filter_state;
} BassState;

void bass_state_init(BassState *state);
void bass_process(BassState *state,
                  const SynthBlockConfig *cfg,
                  float frequency,
                  float *out,
                  size_t frames);

typedef struct {
    float phase_fund;
    float phase_detune;
    float breath_env;
} FluteState;

void flute_state_init(FluteState *state);
void flute_process(FluteState *state,
                   const SynthBlockConfig *cfg,
                   float frequency,
                   float *out,
                   size_t frames);

typedef struct {
    float overtone_envs[4];
    float phases[4];
} PianoState;

void piano_state_init(PianoState *state);
void piano_process(PianoState *state,
                   const SynthBlockConfig *cfg,
                   float base_frequency,
                   float *out,
                   size_t frames);

typedef struct {
    float last_output;
    float delay_line[128];
    size_t delay_index;
    float damping;
} KarplusStrongState;

void ks_state_init(KarplusStrongState *state, float damping, size_t delay_samples);
void ks_process(KarplusStrongState *state,
                const SynthBlockConfig *cfg,
                float excitation_noise,
                float *out,
                size_t frames);

typedef struct {
    float phase;
    float vibrato_phase;
    float env;
} EgtrState;

void egtr_state_init(EgtrState *state);
void egtr_process(EgtrState *state,
                  const SynthBlockConfig *cfg,
                  float frequency,
                  float drive,
                  float *out,
                  size_t frames);

typedef struct {
    float noise_seed;
    float chirp_phase;
    float env;
} BirdsState;

void birds_state_init(BirdsState *state);
void birds_process(BirdsState *state,
                   const SynthBlockConfig *cfg,
                   float *out,
                   size_t frames);

typedef struct {
    float phases[3];
    float env;
} StrPadState;

void strpad_state_init(StrPadState *state);
void strpad_process(StrPadState *state,
                    const SynthBlockConfig *cfg,
                    float base_frequency,
                    float *out,
                    size_t frames);

typedef struct {
    float phases[4];
    float env;
} BellState;

void bell_state_init(BellState *state);
void bell_process(BellState *state,
                  const SynthBlockConfig *cfg,
                  float base_frequency,
                  float *out,
                  size_t frames);

typedef struct {
    float phase;
    float lip_filter;
    float env;
} BrassState;

void brass_state_init(BrassState *state);
void brass_process(BrassState *state,
                   const SynthBlockConfig *cfg,
                   float frequency,
                   float *out,
                   size_t frames);

typedef struct {
    KarplusStrongState ks;
} KalimbaState;

void kalimba_state_init(KalimbaState *state, size_t delay_samples);
void kalimba_process(KalimbaState *state,
                     const SynthBlockConfig *cfg,
                     float excitation,
                     float *out,
                     size_t frames);
