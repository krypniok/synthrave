#define _GNU_SOURCE

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "synthrave/sequence.h"
#include "synthrave/midi_loader.h"
#include "synthrave/scheduler.h"

static bool parse_float(const char *s, float *out) {
    if (!s) {
        return false;
    }
    char *end = NULL;
    float v = strtof(s, &end);
    if (end == s || (end && *end != '\0')) {
        return false;
    }
    *out = v;
    return true;
}

static bool parse_int(const char *s, int *out) {
    if (!s) {
        return false;
    }
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return false;
    }
    *out = (int)v;
    return true;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s [options] token [token...]\n"
            "  %s -f file.aox [options]\n"
            "Options:\n"
            "  -sr <rate>       Sample rate (default 44100)\n"
            "  -g <gain>        Output gain 0..1 (default 0.3)\n"
            "  -l <ms>          Default duration per token (default 120)\n"
            "  -fade <ms>       Fade in/out per tone (default 8)\n"
            "  -f <file>        Sequence file (.srave/.aox)\n"
            "  -espeak <path>   espeak binary for SAY events\n",
            prog, prog);
}

int main(int argc, char **argv) {
    SequenceOptions opts = {
        .sample_rate = 44100,
        .default_duration_ms = 120,
        .fade_ms = 8,
    };
    float gain = 0.3f;
    const char *seq_file = NULL;
    const char *mid_file = NULL;
    const char *espeak_bin = "espeak";

    int idx = 1;
    while (idx < argc) {
        if (strcmp(argv[idx], "-sr") == 0 && idx + 1 < argc) {
            int tmp = 0;
            if (!parse_int(argv[idx + 1], &tmp) || tmp <= 0) {
                fprintf(stderr, "invalid samplerate: %s\n", argv[idx + 1]);
                return 1;
            }
            opts.sample_rate = tmp;
            idx += 2;
            continue;
        }
        if (strcmp(argv[idx], "-g") == 0 && idx + 1 < argc) {
            float tmp = 0.f;
            if (!parse_float(argv[idx + 1], &tmp) || tmp < 0.f) {
                fprintf(stderr, "invalid gain: %s\n", argv[idx + 1]);
                return 1;
            }
            gain = tmp;
            idx += 2;
            continue;
        }
        if (strcmp(argv[idx], "-l") == 0 && idx + 1 < argc) {
            int tmp = 0;
            if (!parse_int(argv[idx + 1], &tmp) || tmp <= 0) {
                fprintf(stderr, "invalid default duration: %s\n", argv[idx + 1]);
                return 1;
            }
            opts.default_duration_ms = tmp;
            idx += 2;
            continue;
        }
        if (strcmp(argv[idx], "-fade") == 0 && idx + 1 < argc) {
            int tmp = 0;
            if (!parse_int(argv[idx + 1], &tmp) || tmp < 0) {
                fprintf(stderr, "invalid fade: %s\n", argv[idx + 1]);
                return 1;
            }
            opts.fade_ms = tmp;
            idx += 2;
            continue;
        }
        if (strcmp(argv[idx], "-f") == 0 && idx + 1 < argc) {
            seq_file = argv[idx + 1];
            idx += 2;
            continue;
        }
        if (strcmp(argv[idx], "-m") == 0 && idx + 1 < argc) {
            mid_file = argv[idx + 1];
            idx += 2;
            continue;
        }
        if (strcmp(argv[idx], "-espeak") == 0 && idx + 1 < argc) {
            espeak_bin = argv[idx + 1];
            idx += 2;
            continue;
        }
        if (argv[idx][0] == '-') {
            usage(argv[0]);
            return 1;
        }
        break;
    }

    SequenceDocument doc = {0};
    bool ok = false;
    if (mid_file) {
        ok = sequence_load_midi(mid_file, &opts, &doc);
    } else if (seq_file) {
        ok = sequence_load_file(seq_file, &opts, &doc);
    } else if (idx < argc) {
        ok = sequence_build_from_tokens((const char *const *)(argv + idx),
                                        argc - idx, &opts, &doc);
    } else {
        usage(argv[0]);
        return 1;
    }
    if (!ok) {
        fprintf(stderr, "failed to parse sequence\n");
        return 1;
    }
    if (doc.total_samples == 0) {
        fprintf(stderr, "sequence empty\n");
        sequence_document_free(&doc);
        return 1;
    }

    int rc = scheduler_play_document(&doc, &opts, gain, espeak_bin);

    sequence_document_free(&doc);
    sample_cache_clear();
    return rc;
}
