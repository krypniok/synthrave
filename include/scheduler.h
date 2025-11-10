#ifndef SYNTHRAVE_SCHEDULER_H
#define SYNTHRAVE_SCHEDULER_H

#include "sequence.h"

#ifdef __cplusplus
extern "C" {
#endif

int scheduler_play_document(const SequenceDocument *doc,
                            const SequenceOptions *opts,
                            float gain,
                            const char *espeak_bin);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHRAVE_SCHEDULER_H */
