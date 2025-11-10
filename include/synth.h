#ifndef SYNTHRAVE_SYNTH_H
#define SYNTHRAVE_SYNTH_H

#include <stddef.h>

#include "instrument.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float start_time; /* seconds */
    float duration;   /* seconds */
    float frequency;  /* Hz */
    float velocity;   /* 0..1 */
} SynthNoteEvent;

typedef struct {
    const SynthInstrument *instrument;
    const SynthNoteEvent *events;
    size_t event_count;
    float gain; /* linear gain 0..1 */
    float pan;  /* -1 left, +1 right */
} SynthTrack;

typedef struct {
    const SynthTrack *tracks;
    size_t track_count;
    float length_seconds; /* optional helper, can be 0 for auto */
} SynthSong;

typedef struct {
    unsigned int sample_rate;
    unsigned int channels; /* 1 or 2 */
} SynthEngine;

size_t synth_engine_frames_for_song(const SynthEngine *engine, const SynthSong *song);
float synth_song_estimate_length(const SynthSong *song);
void synth_engine_render(const SynthEngine *engine,
                         const SynthSong *song,
                         float *buffer,
                         size_t frame_count);
void synth_engine_render_block(const SynthEngine *engine,
                               const SynthSong *song,
                               float start_time,
                               float *buffer,
                               size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHRAVE_SYNTH_H */
