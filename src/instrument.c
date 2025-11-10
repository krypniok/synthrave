#include "instrument.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clampf(float value, float min, float max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static float waveform(SynthInstrumentKind kind, float phase) {
    switch (kind) {
    case SYNTH_INSTRUMENT_SINE:
        return sinf(phase);
    case SYNTH_INSTRUMENT_SQUARE:
        return sinf(phase) >= 0.0f ? 0.8f : -0.8f;
    case SYNTH_INSTRUMENT_SAW: {
        float normalized = phase / (2.0f * (float)M_PI);
        normalized -= floorf(normalized);
        return 2.0f * normalized - 1.0f;
    }
    case SYNTH_INSTRUMENT_TRIANGLE: {
        float normalized = phase / (2.0f * (float)M_PI);
        normalized -= floorf(normalized);
        return 4.0f * fabsf(normalized - 0.5f) - 1.0f;
    }
    default:
        return 0.0f;
    }
}

static float adsr(const SynthInstrument *instrument,
                  float time_since_start,
                  float note_duration) {
    if (instrument == NULL) {
        return 0.0f;
    }

    const float attack = fmaxf(instrument->attack, 1.0e-4f);
    const float decay = fmaxf(instrument->decay, 1.0e-4f);
    const float sustain_level = clampf(instrument->sustain, 0.0f, 1.0f);
    const float release_time = fmaxf(instrument->release, 1.0e-4f);

    if (time_since_start < 0.0f) {
        return 0.0f;
    }

    if (time_since_start < attack) {
        return time_since_start / attack;
    }

    const float decay_start = attack;
    const float decay_end = decay_start + decay;

    if (time_since_start < decay_end) {
        const float decay_progress = (time_since_start - decay_start) / decay;
        return 1.0f + (sustain_level - 1.0f) * decay_progress;
    }

    float sustain_end = note_duration;
    if (sustain_end <= decay_end) {
        sustain_end = decay_end;
    }

    if (time_since_start < sustain_end) {
        return sustain_level;
    }

    const float release_elapsed = time_since_start - sustain_end;
    if (release_elapsed >= release_time) {
        return 0.0f;
    }

    const float release_progress = release_elapsed / release_time;
    return sustain_level * (1.0f - release_progress);
}

float synth_instrument_sample(const SynthInstrument *instrument,
                              float frequency,
                              float time_since_start,
                              float note_duration) {
    const float env = adsr(instrument, time_since_start, note_duration);
    if (env <= 0.0f) {
        return 0.0f;
    }

    const float phase = 2.0f * (float)M_PI * frequency * time_since_start;
    return env * waveform(instrument ? instrument->kind : SYNTH_INSTRUMENT_SINE, phase);
}
