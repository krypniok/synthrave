#include "instruments_ext.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float clampf(float value, float min, float max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

/* LASER ------------------------------------------------------------------- */
void laser_synth_init(LaserSynthState *state, float start_freq, float end_freq, float resonance) {
    if (state == NULL) {
        return;
    }
    state->base_frequency = start_freq;
    state->target_frequency = end_freq;
    state->phase = 0.0f;
    state->sweep_pos = 0.0f;
    state->resonance = resonance;
}

void laser_synth_process(LaserSynthState *state,
                         const SynthBlockConfig *cfg,
                         float *out,
                         size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL || frames == 0u) {
        return;
    }

    const float sample_rate = cfg->sample_rate;
    const float sweep_speed = 1.0f / fmaxf(cfg->block_duration, 0.001f);
    for (size_t i = 0; i < frames; ++i) {
        state->sweep_pos = fminf(1.0f, state->sweep_pos + sweep_speed / (float)frames);
        const float freq = lerp(state->base_frequency, state->target_frequency, state->sweep_pos);
        state->phase += 2.0f * (float)M_PI * freq / sample_rate;
        if (state->phase > 2.0f * (float)M_PI) {
            state->phase -= 2.0f * (float)M_PI;
        }
        const float resonant = sinf(state->phase) * (0.7f + 0.3f * sinf(state->phase * state->resonance));
        out[i] = resonant * (1.0f - state->sweep_pos) + sinf(state->phase * 0.25f) * state->sweep_pos;
    }
}

/* CHOIR ------------------------------------------------------------------- */
void choir_synth_init(ChoirSynthState *state, float root_frequency) {
    if (state == NULL) {
        return;
    }
    state->root_frequency = root_frequency;
    state->detune_cents[0] = -6.0f;
    state->detune_cents[1] = 3.0f;
    state->detune_cents[2] = 7.0f;
    memset(state->phases, 0, sizeof(state->phases));
    state->envelope = 0.0f;
}

static float cents_to_ratio(float cents) {
    return powf(2.0f, cents / 1200.0f);
}

void choir_synth_process(ChoirSynthState *state,
                         const SynthBlockConfig *cfg,
                         float softness,
                         float *out,
                         size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL || frames == 0u) {
        return;
    }

    const float sample_rate = cfg->sample_rate;
    const float attack = fmaxf(0.02f, softness);
    const float release = fmaxf(0.5f, softness * 4.0f);
    const float env_delta = 1.0f / (attack * sample_rate);
    const float env_rel = 1.0f / (release * sample_rate);

    for (size_t i = 0; i < frames; ++i) {
        if (state->envelope < 1.0f) {
            state->envelope = fminf(1.0f, state->envelope + env_delta);
        } else {
            state->envelope = fmaxf(0.6f, state->envelope - env_rel * 0.1f);
        }

        float acc = 0.0f;
        for (size_t v = 0; v < 4; ++v) {
            float ratio = 1.0f;
            if (v > 0) {
                ratio = cents_to_ratio(state->detune_cents[(v - 1) % 3]);
            }
            const float freq = state->root_frequency * ratio;
            state->phases[v] += 2.0f * (float)M_PI * freq / sample_rate;
            if (state->phases[v] > 2.0f * (float)M_PI) {
                state->phases[v] -= 2.0f * (float)M_PI;
            }
            acc += sinf(state->phases[v]) * (v == 0 ? 0.4f : 0.2f);
        }
        const float formant = sinf(state->phases[0] * 3.0f) * 0.15f;
        out[i] = (acc + formant) * (0.4f + 0.6f * state->envelope);
    }
}

/* ANALOG LEAD ------------------------------------------------------------- */
void analog_lead_init(AnalogLeadState *state, float start_frequency, float glide_rate) {
    if (state == NULL) {
        return;
    }
    state->current_frequency = start_frequency;
    state->target_frequency = start_frequency;
    state->glide_rate = glide_rate;
    state->phase = 0.0f;
}

void analog_lead_set_target(AnalogLeadState *state, float target_frequency) {
    if (state == NULL) {
        return;
    }
    state->target_frequency = target_frequency;
}

void analog_lead_process(AnalogLeadState *state,
                         const SynthBlockConfig *cfg,
                         float *out,
                         size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL || frames == 0u) {
        return;
    }

    const float sample_rate = cfg->sample_rate;
    const float glide = clampf(state->glide_rate, 0.0001f, 0.05f);

    for (size_t i = 0; i < frames; ++i) {
        const float diff = state->target_frequency - state->current_frequency;
        state->current_frequency += diff * glide;
        state->phase += 2.0f * (float)M_PI * state->current_frequency / sample_rate;
        if (state->phase > 2.0f * (float)M_PI) {
            state->phase -= 2.0f * (float)M_PI;
        }
        float saw = fmodf(state->phase / (2.0f * (float)M_PI), 1.0f) * 2.0f - 1.0f;
        float pulse = (sinf(state->phase * 2.0f) > 0.0f) ? 0.5f : -0.5f;
        out[i] = 0.7f * saw + 0.3f * pulse;
    }
}

/* SID BASS ---------------------------------------------------------------- */
void sid_bass_init(SidBassState *state, float frequency, float step_duration_ms) {
    if (state == NULL) {
        return;
    }
    state->frequency = frequency;
    state->phase = 0.0f;
    state->step_duration = step_duration_ms / 1000.0f;
    state->time_in_step = 0.0f;
    state->step_index = 0;
}

void sid_bass_process(SidBassState *state,
                      const SynthBlockConfig *cfg,
                      float *out,
                      size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL || frames == 0u) {
        return;
    }

    const float sample_rate = cfg->sample_rate;
    const float step_length = fmaxf(state->step_duration, 0.01f);

    for (size_t i = 0; i < frames; ++i) {
        state->phase += 2.0f * (float)M_PI * state->frequency / sample_rate;
        if (state->phase > 2.0f * (float)M_PI) {
            state->phase -= 2.0f * (float)M_PI;
        }
        float square = sinf(state->phase) >= 0.0f ? 1.0f : -1.0f;
        float step_gain = 0.0f;
        switch (state->step_index % 3) {
        case 0:
            step_gain = 0.9f;
            break;
        case 1:
            step_gain = 0.4f;
            break;
        default:
            step_gain = 0.2f;
            break;
        }
        out[i] = square * step_gain;
        state->time_in_step += 1.0f / sample_rate;
        if (state->time_in_step >= step_length) {
            state->time_in_step -= step_length;
            state->step_index = (state->step_index + 1) % 3;
        }
    }
}

/* CHIP ARP ---------------------------------------------------------------- */
void chip_arp_init(ChipArpState *state, const float *notes_hz, size_t note_count, float tick_ms) {
    if (state == NULL) {
        return;
    }
    state->note_count = note_count > 4 ? 4 : note_count;
    for (size_t i = 0; i < state->note_count; ++i) {
        state->notes_hz[i] = notes_hz ? notes_hz[i] : 440.0f;
    }
    state->current_note = 0;
    state->phase = 0.0f;
    state->tick_duration = fmaxf(tick_ms, 5.0f) / 1000.0f;
    state->tick_time = 0.0f;
}

void chip_arp_process(ChipArpState *state,
                      const SynthBlockConfig *cfg,
                      float *out,
                      size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL || frames == 0u || state->note_count == 0u) {
        return;
    }

    const float sample_rate = cfg->sample_rate;

    for (size_t i = 0; i < frames; ++i) {
        const float freq = state->notes_hz[state->current_note];
        state->phase += 2.0f * (float)M_PI * freq / sample_rate;
        if (state->phase > 2.0f * (float)M_PI) {
            state->phase -= 2.0f * (float)M_PI;
        }
        out[i] = sinf(state->phase) * 0.6f;

        state->tick_time += 1.0f / sample_rate;
        if (state->tick_time >= state->tick_duration) {
            state->tick_time -= state->tick_duration;
            state->current_note = (state->current_note + 1u) % state->note_count;
        }
    }
}

/* ------------------------------------------------------------------------- */
static float frand(void) {
    return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
}

void kick_state_init(KickState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void kick_process(KickState *state,
                  const SynthBlockConfig *cfg,
                  float start_freq,
                  float end_freq,
                  float duration_s,
                  float *out,
                  size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL || frames == 0u) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float sweep_rate = frames > 0 ? 1.0f / fmaxf(duration_s * sample_rate, 1.0f) : 0.0f;
    const float attack_samples = fmaxf(sample_rate * 0.0025f, 1.0f); /* ~2.5 ms ramp to remove clicks */
    for (size_t i = 0; i < frames; ++i) {
        state->sweep_pos = fminf(1.0f, state->sweep_pos + sweep_rate);
        const float freq = lerp(start_freq, end_freq, state->sweep_pos);
        state->phase += 2.0f * (float)M_PI * freq / sample_rate;
        if (state->phase > 2.0f * (float)M_PI) {
            state->phase -= 2.0f * (float)M_PI;
        }
        const float body = sinf(state->phase) * expf(-4.0f * state->sweep_pos);
        state->click_env = fmaxf(0.0f, 1.0f - state->sweep_pos * 8.0f);
        const float click = state->click_env * (frand() * 0.4f + 0.6f);
        float sample = body + click * 0.08f;
        const float attack = fminf(((float)i) / attack_samples, 1.0f);
        out[i] = sample * attack;
    }
}

void snare_state_init(SnareState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->noise_seed = 0.5f;
    state->env_noise = 1.0f;
    state->env_body = 1.0f;
}

void snare_process(SnareState *state,
                   const SynthBlockConfig *cfg,
                   float body_freq,
                   float duration_s,
                   float *out,
                   size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float noise_decay = expf(-1.0f / (sample_rate * fmaxf(duration_s * 0.6f, 0.01f)));
    const float body_decay = expf(-1.0f / (sample_rate * fmaxf(duration_s * 0.3f, 0.01f)));
    for (size_t i = 0; i < frames; ++i) {
        float noise = frand();
        float hp = noise - state->noise_seed;
        state->noise_seed = noise * 0.6f + state->noise_seed * 0.4f;
        float filtered = hp - 0.5f * (hp);
        state->body_phase += 2.0f * (float)M_PI * body_freq / sample_rate;
        if (state->body_phase > 2.0f * (float)M_PI) {
            state->body_phase -= 2.0f * (float)M_PI;
        }
        float body = sinf(state->body_phase);
        out[i] = filtered * state->env_noise * 0.8f + body * state->env_body * 0.4f;
        state->env_noise *= noise_decay;
        state->env_body *= body_decay;
    }
}

void hat_state_init(HatState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->env = 1.0f;
}

void hat_process(HatState *state,
                 const SynthBlockConfig *cfg,
                 float *out,
                 size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float decay = expf(-1.0f / (sample_rate * 0.02f));
    for (size_t i = 0; i < frames; ++i) {
        float noise = frand();
        float hp = noise - 0.6f * state->noise_seed;
        state->noise_seed = noise;
        state->metallic_phase += 2.0f * (float)M_PI * 8000.0f / sample_rate;
        if (state->metallic_phase > 2.0f * (float)M_PI) {
            state->metallic_phase -= 2.0f * (float)M_PI;
        }
        float metallic = sinf(state->metallic_phase) * 0.3f + sinf(state->metallic_phase * 1.5f) * 0.2f;
        out[i] = (hp * 0.7f + metallic) * state->env;
        state->env *= decay;
    }
}

void bass_state_init(BassState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void bass_process(BassState *state,
                  const SynthBlockConfig *cfg,
                  float frequency,
                  float *out,
                  size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float sub_freq = frequency * 0.5f;
    for (size_t i = 0; i < frames; ++i) {
        state->phase_main += 2.0f * (float)M_PI * frequency / sample_rate;
        state->phase_sub += 2.0f * (float)M_PI * sub_freq / sample_rate;
        if (state->phase_main > 2.0f * (float)M_PI) {
            state->phase_main -= 2.0f * (float)M_PI;
        }
        if (state->phase_sub > 2.0f * (float)M_PI) {
            state->phase_sub -= 2.0f * (float)M_PI;
        }
        float saw = fmodf(state->phase_main / (2.0f * (float)M_PI), 1.0f) * 2.0f - 1.0f;
        float sub = sinf(state->phase_sub);
        float mixed = 0.6f * saw + 0.4f * sub;
        state->filter_state = 0.9f * state->filter_state + 0.1f * mixed;
        out[i] = state->filter_state;
    }
}

void flute_state_init(FluteState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void flute_process(FluteState *state,
                   const SynthBlockConfig *cfg,
                   float frequency,
                   float *out,
                   size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float detune = frequency * 1.01f;
    for (size_t i = 0; i < frames; ++i) {
        state->phase_fund += 2.0f * (float)M_PI * frequency / sample_rate;
        state->phase_detune += 2.0f * (float)M_PI * detune / sample_rate;
        if (state->phase_fund > 2.0f * (float)M_PI) {
            state->phase_fund -= 2.0f * (float)M_PI;
        }
        if (state->phase_detune > 2.0f * (float)M_PI) {
            state->phase_detune -= 2.0f * (float)M_PI;
        }
        float fundamental = sinf(state->phase_fund);
        float overtone = 0.3f * sinf(state->phase_detune * 2.0f);
        float breath = frand() * 0.1f;
        out[i] = (fundamental + overtone + breath) * 0.6f;
    }
}

void piano_state_init(PianoState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    for (size_t i = 0; i < 4; ++i) {
        state->overtone_envs[i] = 1.0f;
    }
}

void piano_process(PianoState *state,
                   const SynthBlockConfig *cfg,
                   float base_frequency,
                   float *out,
                   size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float ratios[4] = {1.0f, 2.0f, 3.01f, 4.2f};
    const float decays[4] = {0.6f, 0.4f, 0.2f, 0.15f};
    for (size_t i = 0; i < frames; ++i) {
        float acc = 0.0f;
        for (size_t h = 0; h < 4; ++h) {
            const float freq = base_frequency * ratios[h];
            state->phases[h] += 2.0f * (float)M_PI * freq / sample_rate;
            if (state->phases[h] > 2.0f * (float)M_PI) {
                state->phases[h] -= 2.0f * (float)M_PI;
            }
            acc += sinf(state->phases[h]) * state->overtone_envs[h] * (1.0f / (h + 1));
            state->overtone_envs[h] *= expf(-1.0f / (sample_rate * decays[h]));
        }
        out[i] = acc;
    }
}

void ks_state_init(KarplusStrongState *state, float damping, size_t delay_samples) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->damping = damping;
    const size_t size = sizeof(state->delay_line) / sizeof(state->delay_line[0]);
    state->delay_index = delay_samples % size;
    for (size_t i = 0; i < size; ++i) {
        state->delay_line[i] = frand();
    }
}

void ks_process(KarplusStrongState *state,
                const SynthBlockConfig *cfg,
                float excitation_noise,
                float *out,
                size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const size_t size = sizeof(state->delay_line) / sizeof(state->delay_line[0]);
    for (size_t i = 0; i < frames; ++i) {
        float current = state->delay_line[state->delay_index];
        float next = state->delay_line[(state->delay_index + 1) % size];
        float value = 0.5f * (current + next) * state->damping + excitation_noise * frand() * 0.01f;
        state->delay_line[state->delay_index] = value;
        out[i] = value;
        state->delay_index = (state->delay_index + 1) % size;
    }
}

void egtr_state_init(EgtrState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->env = 1.0f;
}

void egtr_process(EgtrState *state,
                  const SynthBlockConfig *cfg,
                  float frequency,
                  float drive,
                  float *out,
                  size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float decay = expf(-1.0f / (sample_rate * 0.5f));
    for (size_t i = 0; i < frames; ++i) {
        state->phase += 2.0f * (float)M_PI * frequency / sample_rate;
        state->vibrato_phase += 2.0f * (float)M_PI * 5.5f / sample_rate;
        if (state->phase > 2.0f * (float)M_PI) {
            state->phase -= 2.0f * (float)M_PI;
        }
        if (state->vibrato_phase > 2.0f * (float)M_PI) {
            state->vibrato_phase -= 2.0f * (float)M_PI;
        }
        float saw = fmodf(state->phase / (2.0f * (float)M_PI), 1.0f) * 2.0f - 1.0f;
        float square = saw >= 0.0f ? 1.0f : -1.0f;
        float vibrato = 0.01f * sinf(state->vibrato_phase);
        float signal = (0.6f * saw + 0.4f * square) + vibrato + frand() * 0.02f;
        float distorted = tanhf(signal * drive);
        out[i] = distorted * state->env;
        state->env *= decay;
    }
}

void birds_state_init(BirdsState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->env = 1.0f;
}

void birds_process(BirdsState *state,
                   const SynthBlockConfig *cfg,
                   float *out,
                   size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float decay = expf(-1.0f / (sample_rate * 0.3f));
    for (size_t i = 0; i < frames; ++i) {
        state->chirp_phase += 2.0f * (float)M_PI * (4000.0f + 2000.0f * frand()) / sample_rate;
        if (state->chirp_phase > 2.0f * (float)M_PI) {
            state->chirp_phase -= 2.0f * (float)M_PI;
        }
        float chirp = sinf(state->chirp_phase) * (0.5f + 0.5f * frand());
        float noise = frand() * 0.4f;
        out[i] = (chirp + noise) * state->env;
        state->env *= decay;
    }
}

void strpad_state_init(StrPadState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void strpad_process(StrPadState *state,
                    const SynthBlockConfig *cfg,
                    float base_frequency,
                    float *out,
                    size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float attack = expf(-1.0f / (sample_rate * 1.5f));
    const float release = expf(-1.0f / (sample_rate * 3.0f));
    for (size_t i = 0; i < frames; ++i) {
        float blend = (state->env < 1.0f) ? (1.0f - state->env) : (state->env - 1.0f);
        state->env = state->env < 1.0f ? 1.0f - (1.0f - state->env) * attack : state->env * release;
        float acc = 0.0f;
        for (size_t p = 0; p < 3; ++p) {
            float freq = base_frequency * (1.0f + 0.01f * p);
            state->phases[p] += 2.0f * (float)M_PI * freq / sample_rate;
            if (state->phases[p] > 2.0f * (float)M_PI) {
                state->phases[p] -= 2.0f * (float)M_PI;
            }
            acc += sinf(state->phases[p]) * (1.0f / (p + 1));
        }
        out[i] = acc * 0.5f * (0.6f + 0.4f * blend);
    }
}

void bell_state_init(BellState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->env = 1.0f;
}

void bell_process(BellState *state,
                  const SynthBlockConfig *cfg,
                  float base_frequency,
                  float *out,
                  size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float ratios[4] = {1.0f, 2.4f, 3.95f, 5.4f};
    const float decays[4] = {2.0f, 1.2f, 0.8f, 0.6f};
    for (size_t i = 0; i < frames; ++i) {
        float acc = 0.0f;
        for (size_t h = 0; h < 4; ++h) {
            const float freq = base_frequency * ratios[h];
            state->phases[h] += 2.0f * (float)M_PI * freq / sample_rate;
            if (state->phases[h] > 2.0f * (float)M_PI) {
                state->phases[h] -= 2.0f * (float)M_PI;
            }
            acc += sinf(state->phases[h]) * expf(- (float)i / (sample_rate * decays[h]));
        }
        out[i] = acc * 0.5f;
    }
}

void brass_state_init(BrassState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->env = 0.0f;
}

void brass_process(BrassState *state,
                   const SynthBlockConfig *cfg,
                   float frequency,
                   float *out,
                   size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    const float sample_rate = cfg->sample_rate;
    const float attack = 1.0f / (sample_rate * 0.2f);
    const float release = expf(-1.0f / (sample_rate * 0.8f));
    for (size_t i = 0; i < frames; ++i) {
        state->phase += 2.0f * (float)M_PI * frequency / sample_rate;
        if (state->phase > 2.0f * (float)M_PI) {
            state->phase -= 2.0f * (float)M_PI;
        }
        float saw = fmodf(state->phase / (2.0f * (float)M_PI), 1.0f) * 2.0f - 1.0f;
        state->lip_filter = 0.9f * state->lip_filter + 0.1f * saw;
        state->env = fminf(1.0f, state->env + attack);
        out[i] = tanhf(state->lip_filter * 2.0f) * state->env;
        state->env *= release;
    }
}

void kalimba_state_init(KalimbaState *state, size_t delay_samples) {
    if (state == NULL) {
        return;
    }
    ks_state_init(&state->ks, 0.98f, delay_samples);
}

void kalimba_process(KalimbaState *state,
                     const SynthBlockConfig *cfg,
                     float excitation,
                     float *out,
                     size_t frames) {
    if (state == NULL || cfg == NULL || out == NULL) {
        return;
    }
    ks_process(&state->ks, cfg, excitation, out, frames);
}
