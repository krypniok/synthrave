#define _POSIX_C_SOURCE 200809L

#include "scheduler.h"

#include "instruments_ext.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MIX_BLOCK 512

typedef struct {
    SeqSpec spec;
    int channel;
    size_t start_sample;
    size_t total_samples;
    size_t rendered;
    float duration_s;
    union {
        struct {
            float phase;
        } osc;
        struct {
            float phase;
        } glide;
        struct {
            float phases[16];
        } chord;
        struct {
            const SampleData *sample;
            double pos;
            double step;
        } sample;
        KickState kick;
        SnareState snare;
        HatState hat;
        BassState bass;
        FluteState flute;
        PianoState piano;
        KarplusStrongState karplus;
        EgtrState egtr;
        BirdsState birds;
        StrPadState strpad;
        BellState bell;
        BrassState brass;
        KalimbaState kalimba;
        LaserSynthState laser;
        ChoirSynthState choir;
        AnalogLeadState analog;
        SidBassState sid;
        ChipArpState chip;
    } state;
} VoiceRuntime;

typedef struct {
    VoiceRuntime *items;
    size_t len;
    size_t cap;
} VoiceVec;

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
    void *p = realloc(ptr, sz);
    if (!p && sz != 0) {
        fprintf(stderr, "synthrave: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static void voice_vec_push(VoiceVec *vec, const VoiceRuntime *vr) {
    if (vec->len == vec->cap) {
        size_t n = vec->cap ? vec->cap * 2 : 32;
        vec->items = xrealloc(vec->items, n * sizeof(VoiceRuntime));
        vec->cap = n;
    }
    vec->items[vec->len++] = *vr;
}

static bool spec_is_silence(const SeqSpec *sp) {
    return !sp || sp->type == SEQ_SPEC_SILENCE || sp->f_const <= 0.f;
}

static size_t pluck_delay(float freq, int sr) {
    if (freq <= 0.f) {
        freq = 110.f;
    }
    size_t delay = (size_t)((float)sr / freq);
    if (delay < 2) {
        delay = 2;
    }
    if (delay >= 128) {
        delay = 127;
    }
    return delay;
}

static bool voice_init(VoiceRuntime *vr,
                       const SeqToneEvent *tone,
                       const SeqSpec *spec,
                       int channel,
                       int sample_rate) {
    if (!vr || !spec || tone->sample_count == 0 || spec_is_silence(spec)) {
        return false;
    }
    memset(vr, 0, sizeof(*vr));
    vr->spec = *spec;
    vr->channel = channel;
    vr->start_sample = tone->start_sample;
    vr->total_samples = tone->sample_count;
    vr->duration_s = (float)tone->sample_count / (float)sample_rate;

    switch (spec->type) {
        case SEQ_SPEC_SAMPLE:
            if (!spec->sample) {
                return false;
            }
            vr->state.sample.sample = spec->sample;
            vr->state.sample.pos = 0.0;
            vr->state.sample.step =
                (double)spec->sample->length / (double)vr->total_samples;
            break;
        case SEQ_SPEC_KICK:
            kick_state_init(&vr->state.kick);
            break;
        case SEQ_SPEC_SNARE:
            snare_state_init(&vr->state.snare);
            break;
        case SEQ_SPEC_HIHAT:
            hat_state_init(&vr->state.hat);
            break;
        case SEQ_SPEC_BASS:
            bass_state_init(&vr->state.bass);
            break;
        case SEQ_SPEC_FLUTE:
            flute_state_init(&vr->state.flute);
            break;
        case SEQ_SPEC_PIANO:
            piano_state_init(&vr->state.piano);
            break;
        case SEQ_SPEC_GUITAR:
            ks_state_init(&vr->state.karplus, 0.995f, pluck_delay(spec->f_const, sample_rate));
            break;
        case SEQ_SPEC_EGTR:
            egtr_state_init(&vr->state.egtr);
            break;
        case SEQ_SPEC_BIRDS:
            birds_state_init(&vr->state.birds);
            break;
        case SEQ_SPEC_STRPAD:
            strpad_state_init(&vr->state.strpad);
            break;
        case SEQ_SPEC_BELL:
            bell_state_init(&vr->state.bell);
            break;
        case SEQ_SPEC_BRASS:
            brass_state_init(&vr->state.brass);
            break;
        case SEQ_SPEC_KALIMBA:
            kalimba_state_init(&vr->state.kalimba, pluck_delay(spec->f_const, sample_rate));
            break;
        case SEQ_SPEC_LASER:
            laser_synth_init(&vr->state.laser,
                             spec->f_const > 0.f ? spec->f_const : 1320.f,
                             spec->f1 > 0.f ? spec->f1 : spec->f_const * 0.2f,
                             3.0f);
            break;
        case SEQ_SPEC_CHOIR:
            choir_synth_init(&vr->state.choir,
                             spec->f_const > 0.f ? spec->f_const : 261.63f);
            break;
        case SEQ_SPEC_ANALOGLEAD:
            analog_lead_init(&vr->state.analog,
                             spec->f_const > 0.f ? spec->f_const : 440.f,
                             0.02f);
            break;
        case SEQ_SPEC_SIDBASS:
            sid_bass_init(&vr->state.sid,
                          spec->f_const > 0.f ? spec->f_const : 55.f,
                          120.f);
            break;
        case SEQ_SPEC_CHIPARP: {
            float notes[4] = {spec->f_const, spec->f_const * 1.25f,
                              spec->f_const * 1.5f, spec->f_const * 1.75f};
            size_t count = 1;
            if (spec->chord_count > 0) {
                count = (size_t)spec->chord_count;
                if (count > 4) {
                    count = 4;
                }
                for (size_t i = 0; i < count; ++i) {
                    notes[i] = spec->chord[i];
                }
            }
            chip_arp_init(&vr->state.chip, notes, count, 60.f);
            break;
        }
        default:
            break;
    }
    return true;
}

static void voice_render_block(VoiceRuntime *vr,
                               float *dst,
                               size_t frames,
                               int sample_rate) {
    memset(dst, 0, frames * sizeof(float));
    SynthBlockConfig cfg = {
        .sample_rate = (float)sample_rate,
        .block_duration = (float)frames / (float)sample_rate,
    };

    switch (vr->spec.type) {
        case SEQ_SPEC_CONST: {
            float phase = vr->state.osc.phase;
            float step = vr->spec.f_const / (float)sample_rate;
            for (size_t i = 0; i < frames; ++i) {
                phase += step;
                if (phase >= 1.f) {
                    phase -= floorf(phase);
                }
                dst[i] = sinf(2.f * (float)M_PI * phase);
            }
            vr->state.osc.phase = phase;
            break;
        }
        case SEQ_SPEC_GLIDE: {
            float phase = vr->state.glide.phase;
            for (size_t i = 0; i < frames; ++i) {
                float progress = (float)(vr->rendered + i) /
                                 (float)(vr->total_samples > 1 ? vr->total_samples - 1 : 1);
                float freq = vr->spec.f0 + (vr->spec.f1 - vr->spec.f0) * progress;
                phase += freq / (float)sample_rate;
                if (phase >= 1.f) {
                    phase -= floorf(phase);
                }
                dst[i] = sinf(2.f * (float)M_PI * phase);
            }
            vr->state.glide.phase = phase;
            break;
        }
        case SEQ_SPEC_CHORD: {
            float phases[16] = {0};
            memcpy(phases, vr->state.chord.phases, sizeof(phases));
            int count = vr->spec.chord_count;
            if (count <= 0) {
                break;
            }
            for (size_t i = 0; i < frames; ++i) {
                float acc = 0.f;
                for (int h = 0; h < count; ++h) {
                    phases[h] += vr->spec.chord[h] / (float)sample_rate;
                    if (phases[h] >= 1.f) {
                        phases[h] -= floorf(phases[h]);
                    }
                    acc += sinf(2.f * (float)M_PI * phases[h]);
                }
                dst[i] = acc / (float)count;
            }
            memcpy(vr->state.chord.phases, phases, sizeof(phases));
            break;
        }
        case SEQ_SPEC_SAMPLE: {
            const SampleData *sd = vr->state.sample.sample;
            if (!sd || sd->length <= 0) {
                break;
            }
            int ch = vr->spec.sample_channel;
            if (ch >= sd->channels) {
                ch = 0;
            }
            const float *src = sd->chan[ch];
            double pos = vr->state.sample.pos;
            for (size_t i = 0; i < frames; ++i) {
                size_t idx = (size_t)pos;
                float sample;
                if (idx >= (size_t)(sd->length - 1)) {
                    sample = src[sd->length - 1];
                } else {
                    double frac = pos - (double)idx;
                    float a = src[idx];
                    float b = src[idx + 1];
                    sample = (float)(a + (b - a) * frac);
                }
                dst[i] = sample;
                pos += vr->state.sample.step;
            }
            vr->state.sample.pos = pos;
            break;
        }
        case SEQ_SPEC_KICK: {
            float start = vr->spec.f0 > 0.f ? vr->spec.f0 : 140.f;
            float end = vr->spec.f1 > 0.f ? vr->spec.f1 : start * 0.35f;
            kick_process(&vr->state.kick, &cfg, start, end, vr->duration_s, dst, frames);
            break;
        }
        case SEQ_SPEC_SNARE:
            snare_process(&vr->state.snare, &cfg,
                          vr->spec.f_const > 0.f ? vr->spec.f_const : 200.f,
                          vr->duration_s, dst, frames);
            break;
        case SEQ_SPEC_HIHAT:
            hat_process(&vr->state.hat, &cfg, dst, frames);
            break;
        case SEQ_SPEC_BASS:
            bass_process(&vr->state.bass, &cfg, vr->spec.f_const, dst, frames);
            break;
        case SEQ_SPEC_FLUTE:
            flute_process(&vr->state.flute, &cfg, vr->spec.f_const, dst, frames);
            break;
        case SEQ_SPEC_PIANO:
            piano_process(&vr->state.piano, &cfg, vr->spec.f_const, dst, frames);
            break;
        case SEQ_SPEC_GUITAR:
            ks_process(&vr->state.karplus, &cfg, 1.0f, dst, frames);
            break;
        case SEQ_SPEC_EGTR:
            egtr_process(&vr->state.egtr, &cfg, vr->spec.f_const, 3.0f, dst, frames);
            break;
        case SEQ_SPEC_BIRDS:
            birds_process(&vr->state.birds, &cfg, dst, frames);
            break;
        case SEQ_SPEC_STRPAD:
            strpad_process(&vr->state.strpad, &cfg, vr->spec.f_const, dst, frames);
            break;
        case SEQ_SPEC_BELL:
            bell_process(&vr->state.bell, &cfg, vr->spec.f_const, dst, frames);
            break;
        case SEQ_SPEC_BRASS:
            brass_process(&vr->state.brass, &cfg, vr->spec.f_const, dst, frames);
            break;
        case SEQ_SPEC_KALIMBA:
            kalimba_process(&vr->state.kalimba, &cfg, 1.0f, dst, frames);
            break;
        case SEQ_SPEC_LASER:
            laser_synth_process(&vr->state.laser, &cfg, dst, frames);
            break;
        case SEQ_SPEC_CHOIR:
            choir_synth_process(&vr->state.choir, &cfg, 0.4f, dst, frames);
            break;
        case SEQ_SPEC_ANALOGLEAD:
            analog_lead_process(&vr->state.analog, &cfg, dst, frames);
            break;
        case SEQ_SPEC_SIDBASS:
            sid_bass_process(&vr->state.sid, &cfg, dst, frames);
            break;
        case SEQ_SPEC_CHIPARP:
            chip_arp_process(&vr->state.chip, &cfg, dst, frames);
            break;
        default:
            break;
    }
    vr->rendered += frames;
}

static void build_voice_list(const SequenceDocument *doc,
                             int sample_rate,
                             VoiceVec *voices) {
    for (size_t i = 0; i < doc->tone_count; ++i) {
        const SeqToneEvent *tone = &doc->tones[i];
        if (tone->sample_count == 0) {
            continue;
        }
        VoiceRuntime vr;
        if (!spec_is_silence(&tone->left) &&
            voice_init(&vr, tone, &tone->left, 0, sample_rate)) {
            voice_vec_push(voices, &vr);
        }
        bool needs_right = tone->stereo || tone->left.type != tone->right.type;
        if (needs_right && !spec_is_silence(&tone->right) &&
            voice_init(&vr, tone, &tone->right, 1, sample_rate)) {
            voice_vec_push(voices, &vr);
        }
    }
}

static void apply_fade(float *buf, size_t frames, int sr, int fade_ms) {
    if (fade_ms <= 0) {
        return;
    }
    size_t fade = (size_t)((float)fade_ms / 1000.f * sr);
    if (fade * 2 > frames) {
        fade = frames / 2;
    }
    for (size_t i = 0; i < fade; ++i) {
        float g = (float)i / (float)fade;
        buf[i] *= g;
        buf[frames - 1 - i] *= g;
    }
}

static size_t mix_offline(const VoiceVec *voices,
                        const SequenceDocument *doc,
                        const SequenceOptions *opts,
                        float **out_left,
                        float **out_right) {
    size_t total = doc->total_samples;
    float *left = xcalloc(total, sizeof(float));
    float *right = xcalloc(total, sizeof(float));
    float *temp = xmalloc(MIX_BLOCK * sizeof(float));

    for (size_t frame = 0; frame < total; frame += MIX_BLOCK) {
        size_t frames = (frame + MIX_BLOCK > total) ? (total - frame) : MIX_BLOCK;
        for (size_t v = 0; v < voices->len; ++v) {
            VoiceRuntime *vr = &voices->items[v];
            if (vr->rendered >= vr->total_samples) {
                continue;
            }
            size_t voice_start = vr->start_sample;
            size_t voice_end = voice_start + vr->total_samples;
            size_t block_end = frame + frames;
            if (voice_end <= frame || voice_start >= block_end) {
                continue;
            }
            size_t offset = 0;
            if (voice_start > frame) {
                offset = voice_start - frame;
            }
            size_t available = vr->total_samples - vr->rendered;
            size_t to_render = frames - offset;
            if (to_render > available) {
                to_render = available;
            }
            voice_render_block(vr, temp, to_render, opts->sample_rate);
            float *dest = (vr->channel == 0 ? left : right) + frame + offset;
            for (size_t i = 0; i < to_render; ++i) {
                dest[i] += temp[i];
            }
        }
    }

    apply_fade(left, total, opts->sample_rate, opts->fade_ms);
    apply_fade(right, total, opts->sample_rate, opts->fade_ms);

    free(temp);
    *out_left = left;
    *out_right = right;
    return total;
}

static void clamp_and_interleave(int16_t *dst,
                                 const float *L,
                                 const float *R,
                                 size_t n,
                                 float gain) {
    for (size_t i = 0; i < n; ++i) {
        float l = L[i] * gain;
        float r = R[i] * gain;
        if (l > 1.f) l = 1.f;
        if (l < -1.f) l = -1.f;
        if (r > 1.f) r = 1.f;
        if (r < -1.f) r = -1.f;
        dst[2 * i] = (int16_t)lrintf(l * 32767.f);
        dst[2 * i + 1] = (int16_t)lrintf(r * 32767.f);
    }
}

static int64_t now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void sleep_ms(int ms) {
    if (ms <= 0) {
        return;
    }
    struct timespec req = {
        .tv_sec = ms / 1000,
        .tv_nsec = (long)(ms % 1000) * 1000000L,
    };
    nanosleep(&req, NULL);
}

static void speech_event_add_args(char ***argv_ptr,
                                  int *argc_ptr,
                                  const SeqSpeechEvent *ev,
                                  const char *espeak_bin) {
    int count = 1 + (ev->voice ? 2 : 0) + ev->arg_count + 1;
    char **argv = calloc((size_t)count + 1, sizeof(char *));
    if (!argv) {
        return;
    }
    int idx = 0;
    argv[idx++] = (char *)espeak_bin;
    if (ev->voice && *ev->voice) {
        argv[idx++] = "-v";
        argv[idx++] = ev->voice;
    }
    for (int i = 0; i < ev->arg_count; ++i) {
        argv[idx++] = ev->args[i];
    }
    argv[idx++] = ev->text;
    argv[idx] = NULL;
    *argv_ptr = argv;
    *argc_ptr = idx;
}

static void launch_espeak_event(const SeqSpeechEvent *ev, const char *espeak_bin) {
    if (!ev || !ev->text || !*ev->text || !espeak_bin || !*espeak_bin) {
        return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        return;
    }
    if (pid == 0) {
        char **argv = NULL;
        int argc = 0;
        speech_event_add_args(&argv, &argc, ev, espeak_bin);
        if (!argv) {
            _exit(1);
        }
        execvp(espeak_bin, argv);
        _exit(127);
    }
}

static int play_with_openal(const float *L,
                            const float *R,
                            size_t total_samples,
                            float gain,
                            int sr,
                            const SequenceDocument *doc,
                            const char *espeak_bin) {
    if (total_samples == 0) {
        return 0;
    }
    int16_t *pcm = malloc(total_samples * 2 * sizeof(int16_t));
    if (!pcm) {
        fprintf(stderr, "synthrave: cannot allocate pcm buffer\n");
        return 1;
    }
    clamp_and_interleave(pcm, L, R, total_samples, gain);

    ALCdevice *dev = alcOpenDevice(NULL);
    if (!dev) {
        fprintf(stderr, "synthrave: alcOpenDevice failed\n");
        free(pcm);
        return 1;
    }
    ALCcontext *ctx = alcCreateContext(dev, NULL);
    if (!ctx || !alcMakeContextCurrent(ctx)) {
        fprintf(stderr, "synthrave: alcMakeContextCurrent failed\n");
        if (ctx) {
            alcDestroyContext(ctx);
        }
        alcCloseDevice(dev);
        free(pcm);
        return 1;
    }

    ALuint buf = 0, src = 0;
    alGenBuffers(1, &buf);
    alBufferData(buf, AL_FORMAT_STEREO16, pcm, (ALsizei)(total_samples * 2 * sizeof(int16_t)), sr);
    alGenSources(1, &src);
    alSourcei(src, AL_BUFFER, buf);
    alSourcef(src, AL_GAIN, 1.f);
    alSourcePlay(src);

    size_t speech_idx = 0;
    int64_t start = now_ms();
    while (true) {
        if (doc && speech_idx < doc->speech_count) {
            int64_t elapsed = now_ms() - start;
            while (speech_idx < doc->speech_count &&
                   doc->speech[speech_idx].start_ms <= elapsed) {
                launch_espeak_event(&doc->speech[speech_idx], espeak_bin);
                speech_idx++;
            }
        }
        ALint state = 0;
        alGetSourcei(src, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING && (!doc || speech_idx >= doc->speech_count)) {
            break;
        }
        sleep_ms(3);
    }

    alDeleteSources(1, &src);
    alDeleteBuffers(1, &buf);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
    free(pcm);
    return 0;
}

int scheduler_play_document(const SequenceDocument *doc,
                            const SequenceOptions *opts,
                            float gain,
                            const char *espeak_bin) {
    if (!doc || !opts) {
        return 1;
    }
    VoiceVec voices = {0};
    build_voice_list(doc, opts->sample_rate, &voices);

    float *left = NULL;
    float *right = NULL;
    size_t total_samples = doc->total_samples;
    if (voices.len > 0) {
        total_samples = mix_offline(&voices, doc, opts, &left, &right);
    } else {
        size_t total = total_samples;
        if (total == 0) {
            total = (size_t)((float)opts->sample_rate *
                             (opts->default_duration_ms / 1000.f));
            if (total == 0) {
                total = opts->sample_rate;
            }
        }
        if (doc->speech_count == 0) {
            fprintf(stderr, "synthrave: no playable voices\n");
            free(voices.items);
            return 1;
        }
        left = xcalloc(total, sizeof(float));
        right = xcalloc(total, sizeof(float));
        total_samples = total;
    }
    free(voices.items);

    int rc = play_with_openal(left, right, total_samples, gain,
                              opts->sample_rate, doc, espeak_bin);

    free(left);
    free(right);
    return rc;
}
