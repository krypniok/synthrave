#define _GNU_SOURCE

#include "midi_loader.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    double start_ms;
    double duration_ms;
    float freq;
    SeqSpecType spec_type;
    float pan;
} MidiNoteEvent;

typedef struct {
    MidiNoteEvent *items;
    size_t len;
    size_t cap;
} MidiNoteVec;

static void *xcalloc(size_t n, size_t sz) {
    void *ptr = calloc(n, sz);
    if (!ptr) {
        fprintf(stderr, "midi_loader: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void *xrealloc(void *ptr, size_t sz) {
    void *p = realloc(ptr, sz);
    if (!p && sz != 0) {
        fprintf(stderr, "midi_loader: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static uint32_t read_be32(FILE *fp) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4) {
        return 0;
    }
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           (uint32_t)buf[3];
}

static uint16_t read_be16(FILE *fp) {
    uint8_t buf[2];
    if (fread(buf, 1, 2, fp) != 2) {
        return 0;
    }
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

static uint32_t read_varlen(FILE *fp) {
    uint32_t value = 0;
    uint8_t c = 0;
    do {
        if (fread(&c, 1, 1, fp) != 1) {
            return value;
        }
        value = (value << 7) | (c & 0x7F);
    } while (c & 0x80);
    return value;
}

static float midi_note_to_hz(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

static SeqSpecType program_to_spec(uint8_t program) {
    static const SeqSpecType table[] = {
        SEQ_SPEC_PIANO, SEQ_SPEC_PIANO, SEQ_SPEC_PIANO, SEQ_SPEC_PIANO,
        SEQ_SPEC_PIANO, SEQ_SPEC_PIANO, SEQ_SPEC_PIANO, SEQ_SPEC_PIANO,
        SEQ_SPEC_GUITAR, SEQ_SPEC_GUITAR, SEQ_SPEC_GUITAR, SEQ_SPEC_GUITAR,
        SEQ_SPEC_EGTR, SEQ_SPEC_EGTR, SEQ_SPEC_EGTR, SEQ_SPEC_EGTR,
        SEQ_SPEC_BASS, SEQ_SPEC_BASS, SEQ_SPEC_BASS, SEQ_SPEC_BASS,
        SEQ_SPEC_FLUTE, SEQ_SPEC_FLUTE, SEQ_SPEC_FLUTE, SEQ_SPEC_FLUTE,
        SEQ_SPEC_STRPAD, SEQ_SPEC_STRPAD, SEQ_SPEC_STRPAD, SEQ_SPEC_CHOIR,
        SEQ_SPEC_BRASS, SEQ_SPEC_BRASS, SEQ_SPEC_BRASS, SEQ_SPEC_ANALOGLEAD,
        SEQ_SPEC_LASER, SEQ_SPEC_ANALOGLEAD, SEQ_SPEC_CHIPARP, SEQ_SPEC_CHIPARP,
    };
    if (program < sizeof(table) / sizeof(table[0])) {
        return table[program];
    }
    return SEQ_SPEC_PIANO;
}

static void note_vec_push(MidiNoteVec *vec, MidiNoteEvent ev) {
    if (vec->len == vec->cap) {
        size_t n = vec->cap ? vec->cap * 2 : 128;
        vec->items = xrealloc(vec->items, n * sizeof(MidiNoteEvent));
        vec->cap = n;
    }
    vec->items[vec->len++] = ev;
}

typedef struct {
    int active;
    double start_ms;
    SeqSpecType spec;
    float freq;
    float pan;
} ActiveNote;

static float default_pan_for_channel(uint8_t channel) {
    if (channel > 15) {
        channel = 15;
    }
    return ((float)channel / 15.0f) * 2.0f - 1.0f;
}

static void convert_track(FILE *mid,
                          uint32_t track_size,
                          uint16_t division,
                          MidiNoteVec *notes,
                          double *max_end_ms) {
    long track_end = ftell(mid) + (long)track_size;
    double current_ms = 0.0;
    uint8_t running_status = 0;
    uint8_t program_per_channel[16];
    memset(program_per_channel, 0, sizeof(program_per_channel));
    ActiveNote active[16][128];
    memset(active, 0, sizeof(active));
    double tempo_us_per_qn = 500000.0;

    while (ftell(mid) < track_end) {
        uint32_t delta = read_varlen(mid);
        double delta_ms = (double)delta * tempo_us_per_qn / (double)division / 1000.0;
        current_ms += delta_ms;

        uint8_t status = 0;
        if (fread(&status, 1, 1, mid) != 1) {
            break;
        }
        if (status < 0x80) {
            if (!running_status) {
                return;
            }
            fseek(mid, -1, SEEK_CUR);
            status = running_status;
        } else {
            running_status = status;
        }

        if (status == 0xFF) {
            uint8_t meta_type = 0;
            if (fread(&meta_type, 1, 1, mid) != 1) {
                break;
            }
            uint32_t len = read_varlen(mid);
            if (meta_type == 0x51 && len == 3) {
                uint8_t tempo_bytes[3];
                if (fread(tempo_bytes, 1, 3, mid) != 3) {
                    break;
                }
                tempo_us_per_qn = (double)((tempo_bytes[0] << 16) |
                                           (tempo_bytes[1] << 8) |
                                           tempo_bytes[2]);
                if (len > 3) {
                    fseek(mid, len - 3, SEEK_CUR);
                }
            } else {
                fseek(mid, len, SEEK_CUR);
            }
            continue;
        } else if (status == 0xF0 || status == 0xF7) {
            uint32_t len = read_varlen(mid);
            fseek(mid, len, SEEK_CUR);
            continue;
        }

        uint8_t type = status & 0xF0;
        uint8_t channel = status & 0x0F;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
        if (fread(&data1, 1, 1, mid) != 1) {
            break;
        }
        if (type != 0xC0 && type != 0xD0) {
            if (fread(&data2, 1, 1, mid) != 1) {
                break;
            }
        }

        if (type == 0x90 && data2 != 0) {
            ActiveNote *slot = &active[channel][data1];
            if (slot->active) {
                double dur_ms = current_ms - slot->start_ms;
                if (dur_ms < 10.0) {
                    dur_ms = 10.0;
                }
                MidiNoteEvent ev = {
                    .start_ms = slot->start_ms,
                    .duration_ms = dur_ms,
                    .freq = slot->freq,
                    .spec_type = slot->spec,
                    .pan = slot->pan,
                };
                note_vec_push(notes, ev);
                double end_ms = slot->start_ms + dur_ms;
                if (end_ms > *max_end_ms) {
                    *max_end_ms = end_ms;
                }
            }
            slot->active = 1;
            slot->start_ms = current_ms;
            slot->freq = midi_note_to_hz(data1);
            slot->spec = program_to_spec(program_per_channel[channel]);
            slot->pan = default_pan_for_channel(channel);
        } else if ((type == 0x90 && data2 == 0) || type == 0x80) {
            ActiveNote *slot = &active[channel][data1];
            if (slot->active) {
                double dur_ms = current_ms - slot->start_ms;
                if (dur_ms < 10.0) {
                    dur_ms = 10.0;
                }
                MidiNoteEvent ev = {
                    .start_ms = slot->start_ms,
                    .duration_ms = dur_ms,
                    .freq = slot->freq,
                    .spec_type = slot->spec,
                    .pan = slot->pan,
                };
                note_vec_push(notes, ev);
                double end_ms = slot->start_ms + dur_ms;
                if (end_ms > *max_end_ms) {
                    *max_end_ms = end_ms;
                }
                slot->active = 0;
            }
        } else if (type == 0xC0) {
            program_per_channel[channel] = data1;
        }
    }

    for (int ch = 0; ch < 16; ++ch) {
        for (int note = 0; note < 128; ++note) {
            ActiveNote *slot = &active[ch][note];
            if (slot->active) {
                double dur_ms = current_ms - slot->start_ms;
                if (dur_ms < 10.0) {
                    dur_ms = 10.0;
                }
                MidiNoteEvent ev = {
                    .start_ms = slot->start_ms,
                    .duration_ms = dur_ms,
                    .freq = slot->freq,
                    .spec_type = slot->spec,
                    .pan = slot->pan,
                };
                note_vec_push(notes, ev);
                double end_ms = slot->start_ms + dur_ms;
                if (end_ms > *max_end_ms) {
                    *max_end_ms = end_ms;
                }
                slot->active = 0;
            }
        }
    }

    fseek(mid, track_end, SEEK_SET);
}

bool sequence_load_midi(const char *path,
                        const SequenceOptions *opts,
                        SequenceDocument *doc) {
    if (!path || !opts || !doc) {
        return false;
    }
    FILE *mid = fopen(path, "rb");
    if (!mid) {
        fprintf(stderr, "synthrave: cannot open midi %s\n", path);
        return false;
    }
    char chunk[4];
    if (fread(chunk, 1, 4, mid) != 4 || memcmp(chunk, "MThd", 4) != 0) {
        fprintf(stderr, "synthrave: invalid midi header\n");
        fclose(mid);
        return false;
    }
    uint32_t header_size = read_be32(mid);
    uint16_t format = read_be16(mid);
    uint16_t tracks = read_be16(mid);
    uint16_t division = read_be16(mid);
    if (division & 0x8000) {
        fprintf(stderr, "synthrave: SMPTE midi not supported\n");
        fclose(mid);
        return false;
    }
    if (header_size > 6) {
        fseek(mid, header_size - 6, SEEK_CUR);
    }
    if (format > 2) {
        fprintf(stderr, "synthrave: midi format %d unsupported\n", format);
        fclose(mid);
        return false;
    }

    MidiNoteVec notes = {0};
    double max_end_ms = 0.0;
    for (uint16_t t = 0; t < tracks; ++t) {
        if (fread(chunk, 1, 4, mid) != 4 || memcmp(chunk, "MTrk", 4) != 0) {
            fprintf(stderr, "synthrave: missing track chunk\n");
            break;
        }
        uint32_t track_size = read_be32(mid);
        convert_track(mid, track_size, division, &notes, &max_end_ms);
    }
    fclose(mid);

    if (notes.len == 0) {
        free(notes.items);
        fprintf(stderr, "synthrave: midi contained no note events\n");
        return false;
    }

    memset(doc, 0, sizeof(*doc));
    doc->tone_count = notes.len;
    doc->tones = xcalloc(doc->tone_count, sizeof(SeqToneEvent));
    size_t sample_rate = opts->sample_rate;
    double total_ms = max_end_ms + 100.0;
    doc->total_samples = (size_t)((total_ms / 1000.0) * (double)sample_rate);
    if (doc->total_samples < sample_rate / 10) {
        doc->total_samples = sample_rate / 10;
    }

    for (size_t i = 0; i < notes.len; ++i) {
        const MidiNoteEvent *src = &notes.items[i];
        SeqToneEvent *dst = &doc->tones[i];
        memset(dst, 0, sizeof(*dst));
        dst->left.type = src->spec_type;
        dst->left.f_const = src->freq;
        dst->left.f0 = src->freq;
        dst->left.f1 = src->freq;
        dst->right = dst->left;
        dst->stereo = true;
        dst->pan = src->pan;
        dst->gain = 1.0f;
        dst->duration_ms = (int)src->duration_ms;
        dst->sample_count = (size_t)((src->duration_ms / 1000.0) * (double)sample_rate);
        if (dst->sample_count < (size_t)(sample_rate * 0.02)) {
            dst->sample_count = (size_t)(sample_rate * 0.02);
        }
        double start_samples = (src->start_ms / 1000.0) * (double)sample_rate;
        dst->start_sample = (size_t)start_samples;
    }

    free(notes.items);
    return true;
}
