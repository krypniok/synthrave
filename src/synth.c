#include "synthrave/synth.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

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

static float soft_clip(float x) {
    return tanhf(x);
}

static float render_event_sample(const SynthTrack *track,
                                  const SynthNoteEvent *event,
                                  float relative_time) {
    if (track == NULL || track->instrument == NULL || event == NULL) {
        return 0.0f;
    }

    if (relative_time < 0.0f) {
        return 0.0f;
    }

    const float max_time = event->duration + track->instrument->release + 0.01f;
    if (relative_time > max_time) {
        return 0.0f;
    }

    const float sample = synth_instrument_sample(track->instrument,
                                                 event->frequency,
                                                 relative_time,
                                                 event->duration);
    return sample * clampf(event->velocity, 0.0f, 1.0f);
}

float synth_song_estimate_length(const SynthSong *song) {
    if (song == NULL || song->tracks == NULL || song->track_count == 0) {
        return 0.0f;
    }

    float max_length = song->length_seconds;
    for (size_t ti = 0; ti < song->track_count; ++ti) {
        const SynthTrack *track = &song->tracks[ti];
        if (track->events == NULL || track->event_count == 0) {
            continue;
        }
        for (size_t ei = 0; ei < track->event_count; ++ei) {
            const SynthNoteEvent *event = &track->events[ei];
            float instrument_release = track->instrument ? track->instrument->release : 0.0f;
            float end_time = event->start_time + event->duration + instrument_release;
            if (end_time > max_length) {
                max_length = end_time;
            }
        }
    }

    return max_length;
}

size_t synth_engine_frames_for_song(const SynthEngine *engine, const SynthSong *song) {
    if (engine == NULL) {
        return 0;
    }
    const unsigned int sample_rate = engine->sample_rate ? engine->sample_rate : 44100u;
    const float length = synth_song_estimate_length(song);
    return (size_t)(length * (float)sample_rate);
}

void synth_engine_render(const SynthEngine *engine,
                         const SynthSong *song,
                         float *buffer,
                         size_t frame_count) {
    if (engine == NULL || song == NULL || buffer == NULL || frame_count == 0) {
        return;
    }

    const unsigned int sample_rate = engine->sample_rate ? engine->sample_rate : 44100u;
    const unsigned int channels = engine->channels == 1 ? 1u : 2u;

    memset(buffer, 0, sizeof(float) * frame_count * channels);

    for (size_t frame = 0; frame < frame_count; ++frame) {
        const float t = (float)frame / (float)sample_rate;
        float mix_l = 0.0f;
        float mix_r = 0.0f;

        for (size_t ti = 0; ti < song->track_count; ++ti) {
            const SynthTrack *track = &song->tracks[ti];
            if (track->events == NULL || track->event_count == 0) {
                continue;
            }

            float track_sample = 0.0f;
            for (size_t ei = 0; ei < track->event_count; ++ei) {
                const SynthNoteEvent *event = &track->events[ei];
                const float relative_time = t - event->start_time;
                if (relative_time < 0.0f) {
                    continue;
                }
                track_sample += render_event_sample(track, event, relative_time);
            }

            float gain = clampf(track->gain, 0.0f, 2.0f);
            track_sample *= gain;

            const float pan = clampf(track->pan, -1.0f, 1.0f);
            const float left_scale = 0.5f * (2.0f - (pan + 1.0f));
            const float right_scale = 0.5f * (pan + 1.0f);

            mix_l += track_sample * left_scale;
            mix_r += track_sample * right_scale;
        }

        mix_l = soft_clip(mix_l);
        mix_r = soft_clip(mix_r);

        const size_t base_index = frame * channels;
        buffer[base_index] = mix_l;
        if (channels == 2u) {
            buffer[base_index + 1u] = mix_r;
        }
    }
}
