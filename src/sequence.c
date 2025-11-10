#define _GNU_SOURCE
#include "sequence.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    char *cols[5];
} CsvRow;

typedef struct {
    CsvRow *items;
    size_t len;
    size_t cap;
} CsvRowVec;

typedef struct {
    char *name;
    CsvRowVec rows;
} MacroDef;

typedef struct {
    MacroDef *items;
    size_t len;
    size_t cap;
} MacroVec;

typedef struct {
    SeqToneEvent *items;
    size_t len;
    size_t cap;
} ToneVec;

typedef struct {
    SeqSpeechEvent *items;
    size_t len;
    size_t cap;
} SpeechVec;

typedef struct {
    char *path;
    SampleData data;
} SampleCacheEntry;

static SampleCacheEntry *sample_cache = NULL;
static size_t sample_cache_len = 0;
static size_t sample_cache_cap = 0;
static uint32_t noise_state = 0x12345678u;

static void *xcalloc(size_t n, size_t sz) {
    void *ptr = calloc(n, sz);
    if (!ptr) {
        fprintf(stderr, "synthrave: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void *xmalloc(size_t sz) {
    void *ptr = malloc(sz);
    if (!ptr) {
        fprintf(stderr, "synthrave: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void *xrealloc(void *ptr, size_t sz) {
    void *out = realloc(ptr, sz);
    if (!out && sz != 0) {
        fprintf(stderr, "synthrave: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return out;
}

static char *xstrdup(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *dup = xmalloc(len + 1);
    memcpy(dup, s, len + 1);
    return dup;
}

static inline float frand_unit(void) {
    noise_state = noise_state * 1664525u + 1013904223u;
    return ((noise_state >> 8) * (1.0f / 8388608.0f)) * 2.f - 1.f;
}

static void sample_data_free(SampleData *sd) {
    if (!sd) {
        return;
    }
    free(sd->chan[0]);
    free(sd->chan[1]);
    sd->chan[0] = sd->chan[1] = NULL;
    sd->channels = 0;
    sd->length = 0;
    sd->sample_rate = 0;
}

void sample_cache_clear(void) {
    for (size_t i = 0; i < sample_cache_len; ++i) {
        free(sample_cache[i].path);
        sample_data_free(&sample_cache[i].data);
    }
    free(sample_cache);
    sample_cache = NULL;
    sample_cache_len = sample_cache_cap = 0;
}

static char *dup_trimmed(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *cp = xmalloc(len + 1);
    memcpy(cp, s, len + 1);
    size_t i = 0;
    while (isspace((unsigned char)cp[i])) {
        i++;
    }
    size_t j = strlen(cp);
    while (j > i && isspace((unsigned char)cp[j - 1])) {
        j--;
    }
    memmove(cp, cp + i, j - i);
    cp[j - i] = '\0';
    return cp;
}

static void trim_inplace(char *s) {
    if (!s) {
        return;
    }
    size_t len = strlen(s);
    size_t i = 0;
    while (i < len && isspace((unsigned char)s[i])) {
        i++;
    }
    size_t j = len;
    while (j > i && isspace((unsigned char)s[j - 1])) {
        j--;
    }
    memmove(s, s + i, j - i);
    s[j - i] = '\0';
}

static bool parse_float_strict(const char *s, float *out) {
    if (!s || !*s) {
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

static bool parse_int_strict(const char *s, int *out) {
    if (!s || !*s) {
        return false;
    }
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return false;
    }
    if (v < INT32_MIN || v > INT32_MAX) {
        return false;
    }
    *out = (int)v;
    return true;
}

static char *collect_field(const char *start, size_t len) {
    char *buf = xmalloc(len + 1);
    memcpy(buf, start, len);
    buf[len] = '\0';
    trim_inplace(buf);
    return buf;
}

static bool string_is_numeric(const char *s) {
    if (!s || *s == '\0') {
        return false;
    }
    const char *p = s;
    if (*p == '+' || *p == '-') {
        p++;
    }
    bool has_digit = false;
    bool has_dot = false;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            has_digit = true;
            p++;
            continue;
        }
        if (*p == '.' && !has_dot) {
            has_dot = true;
            p++;
            continue;
        }
        return false;
    }
    return has_digit;
}

static void normalize_inline_row(CsvRow *row) {
    if (!row) {
        return;
    }
    if (row->cols[1] && *row->cols[1] && !string_is_numeric(row->cols[1])) {
        if (!row->cols[3] || *row->cols[3] == '\0') {
            row->cols[3] = row->cols[1];
            row->cols[1] = NULL;
        }
    }
    if ((!row->cols[3] || *row->cols[3] == '\0') &&
        row->cols[2] && *row->cols[2] && !string_is_numeric(row->cols[2])) {
        row->cols[3] = row->cols[2];
        row->cols[2] = NULL;
    }
}

static CsvRow csv_row_clone(const CsvRow *src) {
    CsvRow r = {{0}};
    for (int i = 0; i < 5; ++i) {
        if (src->cols[i]) {
            r.cols[i] = xstrdup(src->cols[i]);
        }
    }
    return r;
}

static void csv_row_free(CsvRow *row) {
    if (!row) {
        return;
    }
    for (int i = 0; i < 5; ++i) {
        free(row->cols[i]);
        row->cols[i] = NULL;
    }
}

static void csv_rowvec_push(CsvRowVec *vec, CsvRow row) {
    if (vec->len == vec->cap) {
        size_t n = vec->cap ? vec->cap * 2 : 32;
        vec->items = xrealloc(vec->items, n * sizeof(CsvRow));
        vec->cap = n;
    }
    vec->items[vec->len++] = row;
}

static void csv_rowvec_free(CsvRowVec *vec) {
    if (!vec) {
        return;
    }
    for (size_t i = 0; i < vec->len; ++i) {
        csv_row_free(&vec->items[i]);
    }
    free(vec->items);
    vec->items = NULL;
    vec->len = vec->cap = 0;
}

static void macro_def_free(MacroDef *def) {
    if (!def) {
        return;
    }
    free(def->name);
    csv_rowvec_free(&def->rows);
}

static void macro_vec_push(MacroVec *vec, MacroDef def) {
    if (vec->len == vec->cap) {
        size_t n = vec->cap ? vec->cap * 2 : 8;
        vec->items = xrealloc(vec->items, n * sizeof(MacroDef));
        vec->cap = n;
    }
    vec->items[vec->len++] = def;
}

static void macro_vec_free(MacroVec *vec) {
    if (!vec) {
        return;
    }
    for (size_t i = 0; i < vec->len; ++i) {
        macro_def_free(&vec->items[i]);
    }
    free(vec->items);
    vec->items = NULL;
    vec->len = vec->cap = 0;
}

static char *strdup_upper(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *r = xmalloc(len + 1);
    for (size_t i = 0; i < len; ++i) {
        r[i] = (char)toupper((unsigned char)s[i]);
    }
    r[len] = '\0';
    return r;
}

static MacroDef *macro_find(MacroVec *vec, const char *name) {
    if (!name) {
        return NULL;
    }
    char *upper = strdup_upper(name);
    if (!upper) {
        return NULL;
    }
    MacroDef *result = NULL;
    for (size_t i = 0; i < vec->len; ++i) {
        if (vec->items[i].name && strcasecmp(vec->items[i].name, upper) == 0) {
            result = &vec->items[i];
            break;
        }
    }
    free(upper);
    return result;
}

static bool parse_csv_line(const char *line, CsvRow *row) {
    memset(row, 0, sizeof(*row));
    if (!line) {
        return false;
    }
    const char *p = line;
    int col = 0;
    while (*p && col < 5) {
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p == '\0' || *p == '\n') {
            break;
        }
        if (*p == ',') {
            row->cols[col++] = xstrdup("");
            p++;
            continue;
        }
        const char *start = p;
        if (*p == '"') {
            p++;
            while (*p) {
                if (*p == '"') {
                    if (p[1] == '"') {
                        p += 2;
                        continue;
                    } else {
                        p++;
                        break;
                    }
                }
                p++;
            }
        } else {
            while (*p && *p != ',' && *p != '\n' && *p != '\r') {
                p++;
            }
        }
        size_t len = (size_t)(p - start);
        char *field = collect_field(start, len);
        row->cols[col++] = field;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        while (*p && *p != '\n') {
            if (*p == ',') {
                p++;
                break;
            }
            p++;
        }
    }
    return col > 0;
}

static bool load_wav_file(const char *path, SampleData *out) {
    memset(out, 0, sizeof(*out));
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "synthrave: failed to open wav %s: %s\n", path, strerror(errno));
        return false;
    }
    char id[4];
    if (fread(id, 1, 4, fp) != 4 || memcmp(id, "RIFF", 4) != 0) {
        fprintf(stderr, "synthrave: wav %s missing RIFF\n", path);
        fclose(fp);
        return false;
    }
    uint32_t riff_size = 0;
    if (fread(&riff_size, 4, 1, fp) != 1) {
        fprintf(stderr, "synthrave: wav %s short RIFF chunk\n", path);
        fclose(fp);
        return false;
    }
    if (fread(id, 1, 4, fp) != 4 || memcmp(id, "WAVE", 4) != 0) {
        fprintf(stderr, "synthrave: wav %s missing WAVE\n", path);
        fclose(fp);
        return false;
    }
    uint16_t audio_format = 0, channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;
    bool fmt_found = false, data_found = false;
    uint8_t *raw = NULL;
    size_t raw_frames = 0;
    while (fread(id, 1, 4, fp) == 4) {
        uint32_t chunk_size = 0;
        if (fread(&chunk_size, 4, 1, fp) != 1) {
            break;
        }
        long chunk_start = ftell(fp);
        if (memcmp(id, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                fprintf(stderr, "synthrave: wav %s bad fmt chunk\n", path);
                goto fail;
            }
            if (fread(&audio_format, 2, 1, fp) != 1 ||
                fread(&channels, 2, 1, fp) != 1 ||
                fread(&sample_rate, 4, 1, fp) != 1) {
                fprintf(stderr, "synthrave: wav %s truncated fmt\n", path);
                goto fail;
            }
            uint32_t byte_rate = 0;
            uint16_t block_align = 0;
            if (fread(&byte_rate, 4, 1, fp) != 1 ||
                fread(&block_align, 2, 1, fp) != 1 ||
                fread(&bits_per_sample, 2, 1, fp) != 1) {
                fprintf(stderr, "synthrave: wav %s truncated fmt data\n", path);
                goto fail;
            }
            if (chunk_size > 16) {
                fseek(fp, chunk_size - 16, SEEK_CUR);
            }
            if (audio_format != 1 || (channels != 1 && channels != 2) || bits_per_sample != 16) {
                fprintf(stderr, "synthrave: wav %s unsupported format\n", path);
                goto fail;
            }
            fmt_found = true;
        } else if (memcmp(id, "data", 4) == 0) {
            if (!fmt_found) {
                fprintf(stderr, "synthrave: wav %s data before fmt\n", path);
                goto fail;
            }
            raw = xmalloc(chunk_size);
            if (fread(raw, 1, chunk_size, fp) != chunk_size) {
                fprintf(stderr, "synthrave: wav %s truncated\n", path);
                goto fail;
            }
            raw_frames = chunk_size / (channels * (bits_per_sample / 8));
            data_found = true;
        } else {
            fseek(fp, chunk_size, SEEK_CUR);
        }
        if (chunk_size & 1) {
            fseek(fp, 1, SEEK_CUR);
        }
        if (data_found) {
            break;
        }
        (void)chunk_start;
    }
    fclose(fp);
    if (!fmt_found || !data_found || !raw) {
        fprintf(stderr, "synthrave: wav %s missing chunks\n", path);
        free(raw);
        return false;
    }
    out->channels = channels;
    out->length = (int)raw_frames;
    out->sample_rate = (int)sample_rate;
    for (int c = 0; c < channels; ++c) {
        out->chan[c] = xcalloc(raw_frames, sizeof(float));
    }
    if (channels == 1) {
        out->chan[1] = NULL;
    }
    const int16_t *src = (const int16_t *)raw;
    for (size_t i = 0; i < raw_frames; ++i) {
        for (int c = 0; c < channels; ++c) {
            int16_t v = src[i * channels + c];
            out->chan[c][i] = (float)v / 32768.f;
        }
    }
    free(raw);
    return true;
fail:
    fclose(fp);
    free(raw);
    sample_data_free(out);
    return false;
}

static SampleData *sample_cache_get(const char *path) {
    for (size_t i = 0; i < sample_cache_len; ++i) {
        if (strcmp(sample_cache[i].path, path) == 0) {
            return &sample_cache[i].data;
        }
    }
    SampleCacheEntry entry = {0};
    entry.path = xstrdup(path);
    if (!load_wav_file(path, &entry.data)) {
        free(entry.path);
        return NULL;
    }
    if (sample_cache_len == sample_cache_cap) {
        size_t n = sample_cache_cap ? sample_cache_cap * 2 : 8;
        sample_cache = xrealloc(sample_cache, n * sizeof(*sample_cache));
        sample_cache_cap = n;
    }
    sample_cache[sample_cache_len++] = entry;
    return &sample_cache[sample_cache_len - 1].data;
}

static bool parse_float_or_note(const char *s, float *out);

static bool parse_named_spec(const char *s, SeqSpec *sp) {
    if (!s || !*s) {
        return false;
    }
    char buf[128];
    size_t len = strlen(s);
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    memcpy(buf, s, len);
    buf[len] = '\0';
    trim_inplace(buf);
    char *param = NULL;
    for (char *p = buf; *p; ++p) {
        if (*p == '@' || *p == '=') {
            *p = '\0';
            param = p + 1;
            trim_inplace(param);
            break;
        }
        if (*p == '(') {
            *p = '\0';
            param = p + 1;
            char *end = strrchr(param, ')');
            if (end) {
                *end = '\0';
            }
            trim_inplace(param);
            break;
        }
    }
    if (strncasecmp(buf, "WAV", 3) == 0 || strcasecmp(buf, "SAMPLE") == 0) {
        if (!param || !*param) {
            return false;
        }
        char *path = dup_trimmed(param);
        SampleData *sd = sample_cache_get(path);
        free(path);
        if (!sd) {
            return false;
        }
        sp->type = SEQ_SPEC_SAMPLE;
        sp->sample = sd;
        sp->sample_channel = 0;
        return true;
    }
    struct NamedSpec {
        const char *name;
        SeqSpecType type;
        float default_freq;
    };
    static const struct NamedSpec table[] = {
        {"KICK", SEQ_SPEC_KICK, 140.f},
        {"BD", SEQ_SPEC_KICK, 140.f},
        {"SNARE", SEQ_SPEC_SNARE, 200.f},
        {"SD", SEQ_SPEC_SNARE, 200.f},
        {"HAT", SEQ_SPEC_HIHAT, 8000.f},
        {"HIHAT", SEQ_SPEC_HIHAT, 8000.f},
        {"HH", SEQ_SPEC_HIHAT, 8000.f},
        {"BASS", SEQ_SPEC_BASS, 55.f},
        {"SUB", SEQ_SPEC_BASS, 55.f},
        {"FLUTE", SEQ_SPEC_FLUTE, 523.25f},
        {"PIANO", SEQ_SPEC_PIANO, 440.f},
        {"GUITAR", SEQ_SPEC_GUITAR, 330.f},
        {"GT", SEQ_SPEC_GUITAR, 330.f},
        {"EGTR", SEQ_SPEC_EGTR, 196.f},
        {"EGUITAR", SEQ_SPEC_EGTR, 196.f},
        {"BIRDS", SEQ_SPEC_BIRDS, 6000.f},
        {"STRPAD", SEQ_SPEC_STRPAD, 440.f},
        {"PAD", SEQ_SPEC_STRPAD, 440.f},
        {"BELL", SEQ_SPEC_BELL, 880.f},
        {"BRASS", SEQ_SPEC_BRASS, 330.f},
        {"KALIMBA", SEQ_SPEC_KALIMBA, 392.f},
        {"KORA", SEQ_SPEC_KALIMBA, 392.f},
        {"LASER", SEQ_SPEC_LASER, 1320.f},
        {"CHOIR", SEQ_SPEC_CHOIR, 261.63f},
        {"ANALOGLEAD", SEQ_SPEC_ANALOGLEAD, 440.f},
        {"SIDBASS", SEQ_SPEC_SIDBASS, 55.f},
        {"CHIPARP", SEQ_SPEC_CHIPARP, 523.25f},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (strcasecmp(buf, table[i].name) == 0) {
            float freq = table[i].default_freq;
            if (param && *param) {
                if (table[i].type == SEQ_SPEC_LASER) {
                    char *arrow = strstr(param, "->");
                    if (arrow) {
                        *arrow = '\0';
                        char *rhs = arrow + 2;
                        trim_inplace(param);
                        trim_inplace(rhs);
                        float start = 0.f, end = 0.f;
                        if (parse_float_or_note(param, &start)) {
                            freq = start;
                        }
                        if (parse_float_or_note(rhs, &end)) {
                            sp->f1 = end;
                        }
                    } else {
                        float tmp = 0.f;
                        if (parse_float_or_note(param, &tmp) && tmp > 0.f) {
                            freq = tmp;
                        }
                    }
                } else if (table[i].type == SEQ_SPEC_CHIPARP && strchr(param, '+')) {
                    sp->chord_count = 0;
                    char *dup = dup_trimmed(param);
                    char *save = NULL;
                    char *part = strtok_r(dup, "+", &save);
                    while (part && sp->chord_count < 4) {
                        trim_inplace(part);
                        float val = 0.f;
                        if (parse_float_or_note(part, &val) && val > 0.f) {
                            sp->chord[sp->chord_count++] = val;
                        }
                        part = strtok_r(NULL, "+", &save);
                    }
                    free(dup);
                    if (sp->chord_count > 0) {
                        freq = sp->chord[0];
                    }
                } else {
                    float tmp = 0.f;
                    if (parse_float_or_note(param, &tmp) && tmp > 0.f) {
                        freq = tmp;
                    }
                }
            }
            sp->type = table[i].type;
            sp->f_const = freq;
            return true;
        }
    }
    return false;
}

static bool parse_float_or_note(const char *s, float *out) {
    if (parse_float_strict(s, out)) {
        return true;
    }
    char buf[8];
    size_t len = strlen(s);
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    memcpy(buf, s, len);
    buf[len] = '\0';
    trim_inplace(buf);
    if (len < 2) {
        return false;
    }
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7};
    char note_char = (char)toupper((unsigned char)buf[0]);
    int idx = -1;
    switch (note_char) {
        case 'A': idx = 0; break;
        case 'B': idx = 1; break;
        case 'C': idx = 2; break;
        case 'D': idx = 3; break;
        case 'E': idx = 4; break;
        case 'F': idx = 5; break;
        case 'G': idx = 6; break;
        default: return false;
    }
    int semi = base[idx];
    size_t pos = 1;
    if (buf[pos] == '#') {
        semi += 1;
        pos++;
    } else if (buf[pos] == 'B') {
        semi -= 1;
        pos++;
    }
    if (!isdigit((unsigned char)buf[pos])) {
        return false;
    }
    int oct = buf[pos] - '0';
    int midi = (oct + 1) * 12 + semi;
    *out = 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
    return true;
}

typedef struct {
    SeqSpec left;
    SeqSpec right;
    bool stereo;
    int duration_ms;
    bool explicit_dur;
    bool sample_override;
} Token;

static SeqSpec SeqSpec_make_silence(void) {
    SeqSpec sp = {0};
    sp.type = SEQ_SPEC_SILENCE;
    return sp;
}

static SeqSpec spec_clone(const SeqSpec *src) {
    SeqSpec dst = *src;
    return dst;
}

static void seqspec_free(SeqSpec *sp) {
    (void)sp;
}

static SeqSpec parse_spec(const char *s) {
    SeqSpec sp = SeqSpec_make_silence();
    if (!s || !*s) {
        return sp;
    }
    if ((s[0] == 'r' || s[0] == 'R') && (s[1] == '\0')) {
        return sp;
    }
    SeqSpec named;
    if (parse_named_spec(s, &named)) {
        return named;
    }
    const char *tilde = strchr(s, '~');
    const char *plus = strchr(s, '+');
    if (tilde) {
        char a[64], b[64];
        size_t la = (size_t)(tilde - s);
        if (la >= sizeof(a)) {
            la = sizeof(a) - 1;
        }
        memcpy(a, s, la);
        a[la] = '\0';
        strncpy(b, tilde + 1, sizeof(b) - 1);
        b[sizeof(b) - 1] = '\0';
        trim_inplace(a);
        trim_inplace(b);
        float f0 = 0.f, f1 = 0.f;
        if (!parse_float_or_note(a, &f0) || !parse_float_or_note(b, &f1)) {
            return sp;
        }
        sp.type = SEQ_SPEC_GLIDE;
        sp.f0 = f0;
        sp.f1 = f1;
        return sp;
    }
    if (plus) {
        sp.type = SEQ_SPEC_CHORD;
        sp.chord_count = 0;
        const char *p = s;
        char tmp[64];
        while (*p && sp.chord_count < 16) {
            const char *q = strchr(p, '+');
            size_t ln = q ? (size_t)(q - p) : strlen(p);
            if (ln >= sizeof(tmp)) {
                ln = sizeof(tmp) - 1;
            }
            memcpy(tmp, p, ln);
            tmp[ln] = '\0';
            trim_inplace(tmp);
            float f = 0.f;
            if (parse_float_or_note(tmp, &f)) {
                sp.chord[sp.chord_count++] = f;
            }
            if (!q) {
                break;
            }
            p = q + 1;
        }
        if (sp.chord_count == 0) {
            sp.type = SEQ_SPEC_SILENCE;
        }
        return sp;
    }
    float f = 0.f;
    if (!parse_float_or_note(s, &f) || f <= 0.f) {
        return sp;
    }
    sp.type = SEQ_SPEC_CONST;
    sp.f_const = f;
    return sp;
}

static bool parse_token(const char *arg, int def_ms, Token *out) {
    char *dup = xstrdup(arg);
    char *col = strrchr(dup, ':');
    int dur = def_ms;
    bool explicit_dur = false;
    if (col) {
        *col = '\0';
        char *d = col + 1;
        trim_inplace(d);
        if (parse_int_strict(d, &dur) && dur > 0) {
            explicit_dur = true;
        } else {
            dur = def_ms;
        }
    }
    char *body = dup;
    trim_inplace(body);
    Token t = {0};
    t.duration_ms = dur;
    t.explicit_dur = explicit_dur;
    if ((body[0] == 'r' || body[0] == 'R' || body[0] == '0') && body[1] == '\0') {
        t.left = SeqSpec_make_silence();
        t.right = t.left;
        free(dup);
        *out = t;
        return true;
    }
    char *comma = NULL;
    for (char *p = body; *p; ++p) {
        if (*p == ',') {
            comma = p;
            break;
        }
    }
    if (comma) {
        *comma = '\0';
        char *ls = body;
        char *rs = comma + 1;
        trim_inplace(ls);
        trim_inplace(rs);
        t.left = parse_spec(ls);
        t.right = parse_spec(rs);
        t.stereo = true;
    } else {
        t.left = parse_spec(body);
        t.right = t.left;
        t.stereo = false;
    }
    if (t.left.type == SEQ_SPEC_SAMPLE && t.left.sample) {
        if (t.left.sample->channels > 1) {
            t.stereo = true;
            t.left.sample_channel = 0;
            t.right = t.left;
            t.right.sample_channel = 1;
        } else {
            t.left.sample_channel = 0;
            t.right.sample_channel = 0;
        }
    }
    t.sample_override = (!explicit_dur) &&
                        ((t.left.type == SEQ_SPEC_SAMPLE && t.left.sample) ||
                         (t.right.type == SEQ_SPEC_SAMPLE && t.right.sample));
    free(dup);
    *out = t;
    return true;
}

static int samples_from_ms(int ms, int sr) {
    if (ms <= 0) {
        return 0;
    }
    int64_t prod = (int64_t)sr * ms;
    int samples = (int)(prod / 1000);
    if (samples < 1) {
        samples = 1;
    }
    return samples;
}

static int ms_to_samples_allow_zero(int ms, int sr) {
    if (ms <= 0) {
        return 0;
    }
    int64_t prod = (int64_t)sr * ms;
    return (int)(prod / 1000);
}

static int sample_default_length(const SeqSpec *sp, int sr) {
    if (!sp || sp->type != SEQ_SPEC_SAMPLE || !sp->sample || sp->sample->sample_rate <= 0) {
        return 0;
    }
    double seconds = (double)sp->sample->length / (double)sp->sample->sample_rate;
    int n = (int)lrint(seconds * (double)sr);
    if (n < 1) {
        n = 1;
    }
    return n;
}

static int token_target_samples(const Token *tok, int sr) {
    if (tok->sample_override) {
        int n = sample_default_length(&tok->left, sr);
        if (n <= 0) {
            n = sample_default_length(&tok->right, sr);
        }
        if (n > 0) {
            return n;
        }
    }
    return samples_from_ms(tok->duration_ms, sr);
}

static int parse_gap_ms(const char *s) {
    if (!s || !*s) {
        return 0;
    }
    if (strchr(s, '.')) {
        float secs = 0.f;
        if (parse_float_strict(s, &secs)) {
            int ms = (int)lrintf(secs * 1000.f);
            if (ms < 0) {
                ms = 0;
            }
            return ms;
        }
    }
    int val = 0;
    if (parse_int_strict(s, &val) && val > 0) {
        return val;
    }
    return 0;
}

static char *extract_mode_token(const char *mode_in, bool *is_bg, bool *adv) {
    if (!mode_in || !*mode_in) {
        return NULL;
    }
    char *tmp = xstrdup(mode_in);
    char *save = NULL;
    char *part = strtok_r(tmp, "|", &save);
    char *result = NULL;
    while (part) {
        trim_inplace(part);
        if (*part != '\0') {
            if (strcasecmp(part, "BG") == 0) {
                *is_bg = true;
            } else if (strcasecmp(part, "ADV") == 0) {
                *adv = true;
            } else {
                if (!result) {
                    result = xstrdup(part);
                } else {
                    size_t len = strlen(result);
                    size_t add = strlen(part);
                    result = xrealloc(result, len + add + 2);
                    result[len] = '|';
                    memcpy(result + len + 1, part, add + 1);
                }
            }
        }
        part = strtok_r(NULL, "|", &save);
    }
    free(tmp);
    return result;
}

static void parse_flag_string(const char *flags, bool *is_bg, bool *adv) {
    if (!flags) {
        return;
    }
    char *tmp = xstrdup(flags);
    char *save = NULL;
    char *part = strtok_r(tmp, ",|", &save);
    while (part) {
        trim_inplace(part);
        if (*part != '\0') {
            if (strcasecmp(part, "BG") == 0) {
                *is_bg = true;
            } else if (strcasecmp(part, "ADV") == 0) {
                *adv = true;
            }
        }
        part = strtok_r(NULL, ",|", &save);
    }
    free(tmp);
}

static void tone_vec_push(ToneVec *vec, const SeqToneEvent *ev) {
    if (vec->len == vec->cap) {
        size_t n = vec->cap ? vec->cap * 2 : 32;
        vec->items = xrealloc(vec->items, n * sizeof(SeqToneEvent));
        vec->cap = n;
    }
    vec->items[vec->len++] = *ev;
}

static void speech_vec_push(SpeechVec *vec, const SeqSpeechEvent *ev) {
    if (vec->len == vec->cap) {
        size_t n = vec->cap ? vec->cap * 2 : 16;
        vec->items = xrealloc(vec->items, n * sizeof(SeqSpeechEvent));
        vec->cap = n;
    }
    vec->items[vec->len++] = *ev;
}

static int64_t samples_to_ms(size_t samples, int sr) {
    return (int64_t)(((long double)samples * 1000.0L) / (long double)sr);
}

static void speech_event_add_arg(SeqSpeechEvent *ev, const char *arg) {
    if (!arg || !*arg) {
        return;
    }
    char *dup = xstrdup(arg);
    ev->args = xrealloc(ev->args, sizeof(char *) * (size_t)(ev->arg_count + 1));
    ev->args[ev->arg_count++] = dup;
}

static void speech_event_add_arg_pair(SeqSpeechEvent *ev, const char *flag, const char *value) {
    if (flag) {
        speech_event_add_arg(ev, flag);
    }
    if (value) {
        speech_event_add_arg(ev, value);
    }
}

static void speech_event_clear(SeqSpeechEvent *ev) {
    if (!ev) {
        return;
    }
    free(ev->voice);
    free(ev->text);
    for (int i = 0; i < ev->arg_count; ++i) {
        free(ev->args[i]);
    }
    free(ev->args);
    ev->voice = NULL;
    ev->text = NULL;
    ev->args = NULL;
    ev->arg_count = 0;
}

static void parse_say_options(const char *optstr, SeqSpeechEvent *ev) {
    if (!optstr || !*optstr) {
        return;
    }
    char *tmp = xstrdup(optstr);
    char *save = NULL;
    char *part = strtok_r(tmp, ";", &save);
    while (part) {
        trim_inplace(part);
        if (*part != '\0') {
            char *eq = strchr(part, '=');
            char *key = part;
            char *val = NULL;
            if (eq) {
                *eq = '\0';
                val = eq + 1;
                trim_inplace(key);
                trim_inplace(val);
            } else {
                trim_inplace(key);
            }
            char lower[16] = {0};
            size_t klen = strlen(key);
            for (size_t i = 0; i < klen && i < sizeof(lower) - 1; ++i) {
                lower[i] = (char)tolower((unsigned char)key[i]);
            }
            const char *value = (val && *val) ? val : "1";
            if (strcmp(lower, "text") == 0) {
                if (val && *val && ev->text == NULL) {
                    ev->text = xstrdup(val);
                }
            } else if (strcmp(lower, "s") == 0) {
                speech_event_add_arg_pair(ev, "-s", value);
            } else if (strcmp(lower, "p") == 0) {
                speech_event_add_arg_pair(ev, "-p", value);
            } else if (strcmp(lower, "a") == 0) {
                speech_event_add_arg_pair(ev, "-a", value);
            } else if (strcmp(lower, "g") == 0) {
                speech_event_add_arg_pair(ev, "-g", value);
            } else if (strcmp(lower, "k") == 0) {
                speech_event_add_arg_pair(ev, "-k", value);
            } else if (strcmp(lower, "variant") == 0) {
                if (ev->voice && *value) {
                    size_t len = strlen(ev->voice) + strlen(value) + 1;
                    char *nv = xmalloc(len + 1);
                    snprintf(nv, len + 1, "%s%s", ev->voice, value);
                    free(ev->voice);
                    ev->voice = nv;
                }
            }
        }
        part = strtok_r(NULL, ";", &save);
    }
    free(tmp);
}

static bool parse_say_event(const char *token,
                            size_t start_samples,
                            int sr,
                            SpeechVec *speech) {
    if (!token || strncasecmp(token, "SAY", 3) != 0) {
        return false;
    }
    const char *p = token + 3;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    char *voice = NULL;
    char *opts = NULL;
    if (*p == '@') {
        p++;
        const char *vstart = p;
        while (*p && *p != ':' && *p != ';') {
            p++;
        }
        voice = collect_field(vstart, (size_t)(p - vstart));
    }
    if (*p == ';') {
        p++;
        const char *ostart = p;
        while (*p && *p != ':') {
            p++;
        }
        opts = collect_field(ostart, (size_t)(p - ostart));
    }
    char *text_after_colon = NULL;
    if (*p == ':') {
        p++;
        text_after_colon = dup_trimmed(p);
    }
    SeqSpeechEvent ev = {0};
    ev.start_ms = samples_to_ms(start_samples, sr);
    ev.voice = voice;
    parse_say_options(opts, &ev);
    if (!ev.text && text_after_colon) {
        ev.text = text_after_colon;
        text_after_colon = NULL;
    }
    if (!ev.text || ev.text[0] == '\0') {
        free(opts);
        free(text_after_colon);
        speech_event_clear(&ev);
        return true;
    }
    speech_vec_push(speech, &ev);
    free(opts);
    free(text_after_colon);
    return true;
}

static void set_const_spec(SeqSpec *sp, float freq) {
    if (freq <= 0.f) {
        sp->type = SEQ_SPEC_SILENCE;
        sp->f_const = 0.f;
    } else {
        sp->type = SEQ_SPEC_CONST;
        sp->f_const = freq;
    }
}

static void apply_mode_to_token(Token *tok, const char *mode) {
    if (!mode || !*mode) {
        return;
    }
    float base = (tok->left.type == SEQ_SPEC_CONST) ? tok->left.f_const : 0.f;
    if (strncasecmp(mode, "GLIDE:", 6) == 0) {
        const char *body = mode + 6;
        const char *arrow = strstr(body, "->");
        if (!arrow) {
            return;
        }
        char *left = collect_field(body, (size_t)(arrow - body));
        char *right = dup_trimmed(arrow + 2);
        float f0 = 0.f, f1 = 0.f;
        if (parse_float_or_note(left, &f0) && parse_float_or_note(right, &f1)) {
            tok->left.type = SEQ_SPEC_GLIDE;
            tok->left.f0 = f0;
            tok->left.f1 = f1;
            tok->right = tok->left;
            tok->stereo = false;
        }
        free(left);
        free(right);
        return;
    }
    if (strncasecmp(mode, "UPTO:", 5) == 0 && base > 0.f) {
        float target = 0.f;
        if (parse_float_or_note(mode + 5, &target)) {
            tok->left.type = SEQ_SPEC_GLIDE;
            tok->left.f0 = base;
            tok->left.f1 = target;
            tok->right = tok->left;
            tok->stereo = false;
        }
        return;
    }
    if (strncasecmp(mode, "DOWNTO:", 7) == 0 && base > 0.f) {
        float target = 0.f;
        if (parse_float_or_note(mode + 7, &target)) {
            tok->left.type = SEQ_SPEC_GLIDE;
            tok->left.f0 = base;
            tok->left.f1 = target;
            tok->right = tok->left;
            tok->stereo = false;
        }
        return;
    }
    if (strncasecmp(mode, "UPx:", 4) == 0 && base > 0.f) {
        float ratio = 0.f;
        if (parse_float_strict(mode + 4, &ratio) && ratio > 0.f) {
            tok->left.type = SEQ_SPEC_GLIDE;
            tok->left.f0 = base;
            tok->left.f1 = base * ratio;
            tok->right = tok->left;
            tok->stereo = false;
        }
        return;
    }
    if (strncasecmp(mode, "DOWNx:", 6) == 0 && base > 0.f) {
        float ratio = 0.f;
        if (parse_float_strict(mode + 6, &ratio) && ratio > 0.f) {
            tok->left.type = SEQ_SPEC_GLIDE;
            tok->left.f0 = base;
            tok->left.f1 = base / ratio;
            tok->right = tok->left;
            tok->stereo = false;
        }
        return;
    }
    if (strncasecmp(mode, "BINAURAL:", 9) == 0 && base > 0.f) {
        float delta = 0.f;
        if (parse_float_strict(mode + 9, &delta)) {
            tok->stereo = true;
            set_const_spec(&tok->right, base + delta);
        }
        return;
    }
    float right = 0.f;
    if (parse_float_or_note(mode, &right) && right > 0.f) {
        tok->stereo = true;
        set_const_spec(&tok->right, right);
    }
}

static bool load_sequence_rows(const char *path, CsvRowVec *rows, MacroVec *macros) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "synthrave: cannot open %s: %s\n", path, strerror(errno));
        return false;
    }
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, fp) != -1) {
        char *trim_line = dup_trimmed(line);
        if (!trim_line) {
            continue;
        }
        if (trim_line[0] == '\0' || trim_line[0] == '#' ||
            strncmp(trim_line, "//", 2) == 0 || strncmp(trim_line, "--", 2) == 0) {
            free(trim_line);
            continue;
        }
        if (trim_line[0] == '@') {
            char *brace = strchr(trim_line, '{');
            if (brace) {
                *brace = '\0';
                char *name = trim_line + 1;
                trim_inplace(name);
                MacroDef def = {0};
                def.name = strdup_upper(name);
                bool closed = false;
                while (getline(&line, &cap, fp) != -1) {
                    char *inner = dup_trimmed(line);
                    if (!inner) {
                        continue;
                    }
                    if (inner[0] == '\0' || inner[0] == '#' ||
                        strncmp(inner, "//", 2) == 0 || strncmp(inner, "--", 2) == 0) {
                        free(inner);
                        continue;
                    }
                    if (inner[0] == '}') {
                        free(inner);
                        closed = true;
                        break;
                    }
                    CsvRow row;
                    if (parse_csv_line(line, &row)) {
                        if (row.cols[0] && row.cols[0][0] != '\0') {
                            csv_rowvec_push(&def.rows, row);
                        } else {
                            csv_row_free(&row);
                        }
                    }
                    free(inner);
                }
                if (!closed) {
                    fprintf(stderr, "synthrave: macro %s missing closing brace\n", name);
                    macro_def_free(&def);
                } else {
                    macro_vec_push(macros, def);
                }
                free(trim_line);
                continue;
            }
        }
        free(trim_line);
        CsvRow row;
        if (!parse_csv_line(line, &row)) {
            continue;
        }
        if (!row.cols[0] || row.cols[0][0] == '\0' || row.cols[0][0] == '#') {
            csv_row_free(&row);
            continue;
        }
        csv_rowvec_push(rows, row);
    }
    free(line);
    fclose(fp);
    return true;
}

static bool row_is_repeat_marker(const CsvRow *row, int *span, int *reps) {
    if (!row->cols[0] || row->cols[0][0] != '-') {
        return false;
    }
    int val = 0;
    if (!parse_int_strict(row->cols[0], &val) || val >= 0) {
        return false;
    }
    if (!row->cols[1]) {
        return false;
    }
    int rep = 0;
    if (!parse_int_strict(row->cols[1], &rep) || rep <= 0) {
        return false;
    }
    *span = -val;
    *reps = rep;
    return true;
}

static bool expand_repeats(const CsvRowVec *src, CsvRowVec *dst);
static bool expand_macro_rows(const MacroDef *macro, MacroVec *macros, CsvRowVec *dst, int depth);

static bool expand_macros(const CsvRowVec *src, MacroVec *macros, CsvRowVec *dst) {
    for (size_t i = 0; i < src->len; ++i) {
        const CsvRow *row = &src->items[i];
        const char *tok = row->cols[0];
        if (tok && tok[0] == '@' && tok[1] != '\0') {
            MacroDef *macro = macro_find(macros, tok + 1);
            if (macro) {
                if (!expand_macro_rows(macro, macros, dst, 1)) {
                    return false;
                }
                continue;
            }
        }
        csv_rowvec_push(dst, csv_row_clone(row));
    }
    return true;
}

static bool expand_macro_rows(const MacroDef *macro, MacroVec *macros, CsvRowVec *dst, int depth) {
    if (depth > 16) {
        fprintf(stderr, "synthrave: macro recursion too deep for %s\n", macro->name);
        return false;
    }
    for (size_t i = 0; i < macro->rows.len; ++i) {
        const CsvRow *row = &macro->rows.items[i];
        const char *tok = row->cols[0];
        if (tok && tok[0] == '@' && tok[1] != '\0') {
            MacroDef *inner = macro_find(macros, tok + 1);
            if (!inner) {
                fprintf(stderr, "synthrave: unknown macro %s inside %s\n", tok, macro->name);
                return false;
            }
            if (!expand_macro_rows(inner, macros, dst, depth + 1)) {
                return false;
            }
            continue;
        }
        csv_rowvec_push(dst, csv_row_clone(row));
    }
    return true;
}

static bool expand_repeats(const CsvRowVec *src, CsvRowVec *dst) {
    size_t i = 0;
    while (i < src->len) {
        int span = 0, reps = 0;
        const CsvRow *row = &src->items[i];
        if (row_is_repeat_marker(row, &span, &reps)) {
            if (span <= 0 || reps <= 0) {
                i++;
                continue;
            }
            size_t start = i + 1;
            size_t remaining = (start < src->len) ? (src->len - start) : 0;
            if (remaining >= (size_t)span) {
                CsvRowVec block = {0}, block_expanded = {0};
                for (size_t k = 0; k < (size_t)span; ++k) {
                    csv_rowvec_push(&block, csv_row_clone(&src->items[start + k]));
                }
                expand_repeats(&block, &block_expanded);
                for (int r = 0; r < reps; ++r) {
                    for (size_t k = 0; k < block_expanded.len; ++k) {
                        csv_rowvec_push(dst, csv_row_clone(&block_expanded.items[k]));
                    }
                }
                csv_rowvec_free(&block_expanded);
                csv_rowvec_free(&block);
                i = start + (size_t)span;
                continue;
            } else if (dst->len > 0) {
                if ((size_t)span > dst->len) {
                    span = (int)dst->len;
                }
                size_t base = dst->len - (size_t)span;
                for (int r = 0; r < reps; ++r) {
                    for (size_t k = 0; k < (size_t)span; ++k) {
                        csv_rowvec_push(dst, csv_row_clone(&dst->items[base + k]));
                    }
                }
            }
            i++;
            continue;
        }
        csv_rowvec_push(dst, csv_row_clone(row));
        i++;
    }
    return true;
}

static bool build_sequence_events(const CsvRowVec *rows,
                                  const SequenceOptions *opts,
                                  ToneVec *tones,
                                  SpeechVec *speech,
                                  size_t *total_samples) {
    int sr = opts->sample_rate;
    size_t timeline_samples = 0;
    size_t max_end = 0;
    for (size_t i = 0; i < rows->len; ++i) {
        const CsvRow *row = &rows->items[i];
        const char *tok = row->cols[0] ? row->cols[0] : "";
        if (!tok || !*tok) {
            continue;
        }
        bool is_bg = false;
        bool adv = false;
        char *mode_clean = extract_mode_token(row->cols[3], &is_bg, &adv);
        parse_flag_string(row->cols[4], &is_bg, &adv);
        int dur_ms = opts->default_duration_ms;
        if (row->cols[1] && *row->cols[1]) {
            if (!parse_int_strict(row->cols[1], &dur_ms) || dur_ms <= 0) {
                dur_ms = opts->default_duration_ms;
            }
        }
        int gap_ms = parse_gap_ms(row->cols[2]);
        int gap_samples = ms_to_samples_allow_zero(gap_ms, sr);
        size_t event_start = timeline_samples;
        if (parse_say_event(tok, timeline_samples, sr, speech)) {
            bool advance = (!is_bg || adv);
            if (advance) {
                int say_samples = ms_to_samples_allow_zero((dur_ms > 0) ? dur_ms : opts->default_duration_ms, sr);
                timeline_samples += (size_t)say_samples;
                timeline_samples += (size_t)gap_samples;
            }
            free(mode_clean);
            continue;
        }
        int effective_ms = (dur_ms > 0) ? dur_ms : opts->default_duration_ms;
        Token parsed;
        if (!parse_token(tok, effective_ms, &parsed)) {
            fprintf(stderr, "synthrave: token parse error: %s\n", tok);
            free(mode_clean);
            return false;
        }
        int tone_ms = parsed.duration_ms > 0 ? parsed.duration_ms : effective_ms;
        apply_mode_to_token(&parsed, mode_clean);
        int tone_samples = token_target_samples(&parsed, sr);
        bool advance = (!is_bg || adv);
        if (parsed.left.type == SEQ_SPEC_SILENCE && parsed.right.type == SEQ_SPEC_SILENCE) {
            if (advance) {
                int rest_samples = ms_to_samples_allow_zero(tone_ms, sr);
                timeline_samples += (size_t)rest_samples;
                timeline_samples += (size_t)gap_samples;
            }
            free(mode_clean);
            continue;
        }
        SeqToneEvent ev = {0};
        ev.left = spec_clone(&parsed.left);
        ev.right = spec_clone(&parsed.right);
        ev.stereo = parsed.stereo;
        ev.duration_ms = tone_ms;
        ev.gap_ms = gap_ms;
        ev.explicit_duration = parsed.explicit_dur;
        ev.sample_override = parsed.sample_override;
        ev.start_sample = event_start;
        ev.sample_count = (size_t)tone_samples;
        ev.is_bg = is_bg;
        ev.adv = adv;
        ev.mode_raw = mode_clean ? xstrdup(mode_clean) : NULL;
        ev.flags_raw = row->cols[4] ? xstrdup(row->cols[4]) : NULL;
        tone_vec_push(tones, &ev);
        size_t end = ev.start_sample + ev.sample_count;
        if (end > max_end) {
            max_end = end;
        }
        if (advance) {
            timeline_samples += ev.sample_count;
            timeline_samples += (size_t)gap_samples;
        }
        free(mode_clean);
    }
    if (max_end < timeline_samples) {
        max_end = timeline_samples;
    }
    *total_samples = max_end;
    return true;
}

bool sequence_load_file(const char *path,
                        const SequenceOptions *opts,
                        SequenceDocument *doc) {
    if (!path || !opts || !doc) {
        return false;
    }
    memset(doc, 0, sizeof(*doc));
    CsvRowVec raw = {0}, macro_applied = {0}, expanded = {0};
    MacroVec macros = {0};
    ToneVec tones = {0};
    SpeechVec speech = {0};
    bool ok = false;
    if (!load_sequence_rows(path, &raw, &macros)) {
        goto cleanup;
    }
    if (!expand_macros(&raw, &macros, &macro_applied)) {
        goto cleanup;
    }
    if (!expand_repeats(&macro_applied, &expanded)) {
        goto cleanup;
    }
    size_t total_samples = 0;
    if (!build_sequence_events(&expanded, opts, &tones, &speech, &total_samples)) {
        goto cleanup;
    }
    doc->tones = tones.items;
    doc->tone_count = tones.len;
    doc->speech = speech.items;
    doc->speech_count = speech.len;
    doc->total_samples = total_samples;
    tones.items = NULL;
    speech.items = NULL;
    ok = true;

cleanup:
    csv_rowvec_free(&raw);
    csv_rowvec_free(&macro_applied);
    csv_rowvec_free(&expanded);
    macro_vec_free(&macros);
    if (!ok) {
        for (size_t i = 0; i < tones.len; ++i) {
            seqspec_free(&tones.items[i].left);
            seqspec_free(&tones.items[i].right);
            free(tones.items[i].mode_raw);
            free(tones.items[i].flags_raw);
        }
        free(tones.items);
        for (size_t i = 0; i < speech.len; ++i) {
            speech_event_clear(&speech.items[i]);
        }
        free(speech.items);
    }
    return ok;
}

void sequence_document_free(SequenceDocument *doc) {
    if (!doc) {
        return;
    }
    for (size_t i = 0; i < doc->tone_count; ++i) {
        seqspec_free(&doc->tones[i].left);
        seqspec_free(&doc->tones[i].right);
        free(doc->tones[i].mode_raw);
        free(doc->tones[i].flags_raw);
    }
    free(doc->tones);
    doc->tones = NULL;
    doc->tone_count = 0;
    for (size_t i = 0; i < doc->speech_count; ++i) {
        speech_event_clear(&doc->speech[i]);
    }
    free(doc->speech);
    doc->speech = NULL;
    doc->speech_count = 0;
    doc->total_samples = 0;
}

bool sequence_build_from_tokens(const char *const *tokens,
                                int token_count,
                                const SequenceOptions *opts,
                                SequenceDocument *doc) {
    if (!tokens || token_count <= 0 || !opts || !doc) {
        return false;
    }
    memset(doc, 0, sizeof(*doc));
    CsvRowVec rows = {0};
    ToneVec tones = {0};
    SpeechVec speech = {0};
    size_t total_samples = 0;
    bool ok = false;
    for (int i = 0; i < token_count; ++i) {
        const char *raw = tokens[i];
        if (!raw || !*raw) {
            continue;
        }
        CsvRow row;
        if (!parse_csv_line(raw, &row)) {
            fprintf(stderr, "synthrave: token parse error: %s\n", raw);
            goto cleanup;
        }
        if (!row.cols[0] || row.cols[0][0] == '\0') {
            csv_row_free(&row);
            continue;
        }
        normalize_inline_row(&row);
        csv_rowvec_push(&rows, row);
    }
    if (rows.len == 0) {
        goto cleanup;
    }
    if (!build_sequence_events(&rows, opts, &tones, &speech, &total_samples)) {
        goto cleanup;
    }
    doc->tones = tones.items;
    doc->tone_count = tones.len;
    doc->speech = speech.items;
    doc->speech_count = speech.len;
    doc->total_samples = total_samples;
    tones.items = NULL;
    speech.items = NULL;
    ok = true;

cleanup:
    csv_rowvec_free(&rows);
    if (!ok) {
        for (size_t i = 0; i < tones.len; ++i) {
            seqspec_free(&tones.items[i].left);
            seqspec_free(&tones.items[i].right);
            free(tones.items[i].mode_raw);
            free(tones.items[i].flags_raw);
        }
        free(tones.items);
        for (size_t i = 0; i < speech.len; ++i) {
            speech_event_clear(&speech.items[i]);
        }
        free(speech.items);
    }
    return ok;
}
