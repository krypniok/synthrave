#ifndef SYNTHRAVE_SEQUENCE_H
#define SYNTHRAVE_SEQUENCE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SEQ_SPEC_SILENCE = 0,
    SEQ_SPEC_CONST,
    SEQ_SPEC_GLIDE,
    SEQ_SPEC_CHORD,
    SEQ_SPEC_KICK,
    SEQ_SPEC_SNARE,
    SEQ_SPEC_HIHAT,
    SEQ_SPEC_BASS,
    SEQ_SPEC_FLUTE,
    SEQ_SPEC_PIANO,
    SEQ_SPEC_GUITAR,
    SEQ_SPEC_EGTR,
    SEQ_SPEC_SAMPLE,
    SEQ_SPEC_BIRDS,
    SEQ_SPEC_STRPAD,
    SEQ_SPEC_BELL,
    SEQ_SPEC_BRASS,
    SEQ_SPEC_KALIMBA,
    SEQ_SPEC_LASER,
    SEQ_SPEC_CHOIR,
    SEQ_SPEC_ANALOGLEAD,
    SEQ_SPEC_SIDBASS,
    SEQ_SPEC_CHIPARP
} SeqSpecType;

typedef struct SampleData SampleData;

struct SampleData {
    float *chan[2];
    int channels;
    int length;
    int sample_rate;
};

typedef struct {
    SeqSpecType type;
    float f_const;
    float f0;
    float f1;
    float chord[16];
    int chord_count;
    SampleData *sample;
    int sample_channel;
} SeqSpec;

typedef struct {
    SeqSpec left;
    SeqSpec right;
    bool stereo;
    int duration_ms;
    int gap_ms;
    bool explicit_duration;
    bool sample_override;
    size_t start_sample;
    size_t sample_count;
    bool is_bg;
    bool adv;
    char *mode_raw;
    char *flags_raw;
} SeqToneEvent;

typedef struct {
    int64_t start_ms;
    char *voice;
    char *text;
    char **args;
    int arg_count;
} SeqSpeechEvent;

typedef struct {
    SeqToneEvent *tones;
    size_t tone_count;
    SeqSpeechEvent *speech;
    size_t speech_count;
    size_t total_samples;
} SequenceDocument;

typedef struct {
    int sample_rate;
    int default_duration_ms;
    int fade_ms;
} SequenceOptions;

bool sequence_load_file(const char *path,
                        const SequenceOptions *opts,
                        SequenceDocument *doc);
bool sequence_build_from_tokens(const char *const *tokens,
                                int token_count,
                                const SequenceOptions *opts,
                                SequenceDocument *doc);

void sequence_document_free(SequenceDocument *doc);

void sample_cache_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHRAVE_SEQUENCE_H */
