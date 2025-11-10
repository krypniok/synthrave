#ifndef SYNTHRAVE_MIDI_LOADER_H
#define SYNTHRAVE_MIDI_LOADER_H

#include <stdbool.h>

#include "synthrave/sequence.h"

#ifdef __cplusplus
extern "C" {
#endif

bool sequence_load_midi(const char *path,
                        const SequenceOptions *opts,
                        SequenceDocument *doc);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHRAVE_MIDI_LOADER_H */
