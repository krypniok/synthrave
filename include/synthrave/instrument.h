#ifndef SYNTHRAVE_INSTRUMENT_H
#define SYNTHRAVE_INSTRUMENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Kinds of built-in instruments supported by the synth. */
typedef enum {
    SYNTH_INSTRUMENT_SINE,
    SYNTH_INSTRUMENT_SQUARE,
    SYNTH_INSTRUMENT_SAW,
    SYNTH_INSTRUMENT_TRIANGLE,
} SynthInstrumentKind;

/** Basic ADSR instrument description. */
typedef struct {
    SynthInstrumentKind kind;
    float attack;   /* seconds */
    float decay;    /* seconds */
    float sustain;  /* linear 0..1 */
    float release;  /* seconds */
} SynthInstrument;

float synth_instrument_sample(const SynthInstrument *instrument,
                              float frequency,
                              float time_since_start,
                              float note_duration);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHRAVE_INSTRUMENT_H */
