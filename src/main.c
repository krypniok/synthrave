#define _POSIX_C_SOURCE 200809L

#include <AL/al.h>
#include <AL/alc.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "synthrave/instrument.h"
#include "synthrave/ringbuffer.h"
#include "synthrave/synth.h"

#define STREAM_BUFFER_COUNT 4u
#define STREAM_CHUNK_FRAMES 1024u
#define RING_BUFFER_MULTIPLIER 4u
#define RING_BUFFER_FRAMES (STREAM_BUFFER_COUNT * STREAM_CHUNK_FRAMES * RING_BUFFER_MULTIPLIER)

typedef struct {
    AudioRingBuffer ring;
    float render_cursor;
    float song_length;
    int finished_render;
} StreamContext;

static const SynthInstrument INSTR_BASS = {
    .kind = SYNTH_INSTRUMENT_SINE,
    .attack = 0.01f,
    .decay = 0.2f,
    .sustain = 0.7f,
    .release = 0.3f,
};

static const SynthInstrument INSTR_LEAD = {
    .kind = SYNTH_INSTRUMENT_SQUARE,
    .attack = 0.02f,
    .decay = 0.1f,
    .sustain = 0.5f,
    .release = 0.2f,
};

static const SynthInstrument INSTR_PAD = {
    .kind = SYNTH_INSTRUMENT_SAW,
    .attack = 0.2f,
    .decay = 0.5f,
    .sustain = 0.6f,
    .release = 1.2f,
};

static const SynthInstrument INSTR_PLUCK = {
    .kind = SYNTH_INSTRUMENT_TRIANGLE,
    .attack = 0.005f,
    .decay = 0.08f,
    .sustain = 0.3f,
    .release = 0.12f,
};

static const SynthNoteEvent TRACK_BASS_EVENTS[] = {
    {0.0f, 0.8f, 55.0f, 0.95f},
    {1.0f, 0.8f, 73.416f, 0.95f},
    {2.0f, 0.8f, 82.407f, 0.95f},
    {3.0f, 0.8f, 65.406f, 0.95f},
    {4.0f, 0.8f, 55.0f, 0.95f},
    {5.0f, 0.8f, 73.416f, 0.95f},
    {6.0f, 0.8f, 87.307f, 0.95f},
    {7.0f, 0.8f, 65.406f, 0.95f},
};

static const SynthNoteEvent TRACK_LEAD_EVENTS[] = {
    {0.0f, 0.25f, 440.0f, 0.7f},
    {0.25f, 0.25f, 493.883f, 0.7f},
    {0.5f, 0.25f, 523.251f, 0.7f},
    {0.75f, 0.35f, 659.255f, 0.7f},
    {1.4f, 0.25f, 587.33f, 0.7f},
    {1.65f, 0.25f, 659.255f, 0.7f},
    {1.9f, 0.35f, 783.991f, 0.7f},
    {2.4f, 0.25f, 698.456f, 0.7f},
    {2.65f, 0.25f, 783.991f, 0.7f},
    {2.9f, 0.35f, 880.0f, 0.7f},
    {3.6f, 0.25f, 932.328f, 0.7f},
    {3.85f, 0.25f, 880.0f, 0.7f},
    {4.1f, 0.25f, 783.991f, 0.7f},
    {4.35f, 0.35f, 698.456f, 0.7f},
    {5.0f, 0.25f, 659.255f, 0.7f},
    {5.25f, 0.25f, 587.33f, 0.7f},
    {5.5f, 0.35f, 523.251f, 0.7f},
    {6.1f, 0.25f, 440.0f, 0.7f},
    {6.35f, 0.25f, 523.251f, 0.7f},
    {6.6f, 0.35f, 587.33f, 0.7f},
};

static const SynthNoteEvent TRACK_PAD_EVENTS[] = {
    {0.0f, 4.0f, 220.0f, 0.5f},
    {0.0f, 4.0f, 261.626f, 0.4f},
    {0.0f, 4.0f, 329.628f, 0.4f},
    {4.0f, 4.0f, 207.652f, 0.5f},
    {4.0f, 4.0f, 246.942f, 0.4f},
    {4.0f, 4.0f, 311.127f, 0.4f},
};

static const SynthNoteEvent TRACK_PLUCK_EVENTS[] = {
    {0.0f, 0.15f, 880.0f, 0.8f},
    {0.5f, 0.15f, 987.767f, 0.8f},
    {1.0f, 0.15f, 1046.5f, 0.8f},
    {1.5f, 0.15f, 1174.66f, 0.8f},
    {2.0f, 0.15f, 1318.51f, 0.8f},
    {2.5f, 0.15f, 1396.91f, 0.8f},
    {3.0f, 0.15f, 1567.98f, 0.8f},
    {3.5f, 0.15f, 1760.0f, 0.8f},
    {4.0f, 0.15f, 1975.53f, 0.8f},
    {4.5f, 0.15f, 2093.0f, 0.8f},
    {5.0f, 0.15f, 2349.32f, 0.8f},
    {5.5f, 0.15f, 2637.02f, 0.8f},
    {6.0f, 0.15f, 2793.83f, 0.8f},
    {6.5f, 0.15f, 3135.96f, 0.8f},
    {7.0f, 0.15f, 3520.0f, 0.8f},
};

static const SynthTrack SONG_TRACKS[] = {
    {
        .instrument = &INSTR_BASS,
        .events = TRACK_BASS_EVENTS,
        .event_count = sizeof(TRACK_BASS_EVENTS) / sizeof(TRACK_BASS_EVENTS[0]),
        .gain = 0.9f,
        .pan = -0.2f,
    },
    {
        .instrument = &INSTR_PAD,
        .events = TRACK_PAD_EVENTS,
        .event_count = sizeof(TRACK_PAD_EVENTS) / sizeof(TRACK_PAD_EVENTS[0]),
        .gain = 0.5f,
        .pan = 0.0f,
    },
    {
        .instrument = &INSTR_LEAD,
        .events = TRACK_LEAD_EVENTS,
        .event_count = sizeof(TRACK_LEAD_EVENTS) / sizeof(TRACK_LEAD_EVENTS[0]),
        .gain = 0.6f,
        .pan = 0.35f,
    },
    {
        .instrument = &INSTR_PLUCK,
        .events = TRACK_PLUCK_EVENTS,
        .event_count = sizeof(TRACK_PLUCK_EVENTS) / sizeof(TRACK_PLUCK_EVENTS[0]),
        .gain = 0.4f,
        .pan = -0.4f,
    },
};

static const SynthSong DEMO_SONG = {
    .tracks = SONG_TRACKS,
    .track_count = sizeof(SONG_TRACKS) / sizeof(SONG_TRACKS[0]),
    .length_seconds = 8.5f,
};

static void sleep_millis(unsigned int millis) {
    const struct timespec request = {
        .tv_sec = millis / 1000u,
        .tv_nsec = (long)(millis % 1000u) * 1000000L,
    };
    nanosleep(&request, NULL);
}

static void refill_ring(StreamContext *ctx,
                        const SynthEngine *engine,
                        const SynthSong *song,
                        float *scratch,
                        size_t scratch_frames) {
    if (ctx == NULL || engine == NULL || song == NULL || scratch == NULL || scratch_frames == 0u) {
        return;
    }
    if (ctx->finished_render) {
        return;
    }

    const float sample_rate = (float)(engine->sample_rate ? engine->sample_rate : 44100u);

    while (!ctx->finished_render) {
        size_t space = audio_ring_buffer_space(&ctx->ring);
        if (space == 0u) {
            break;
        }
        size_t frames = space < scratch_frames ? space : scratch_frames;
        synth_engine_render_block(engine, song, ctx->render_cursor, scratch, frames);
        const size_t written = audio_ring_buffer_write(&ctx->ring, scratch, frames);
        if (written == 0u) {
            break;
        }
        ctx->render_cursor += (float)written / sample_rate;
        if (ctx->render_cursor >= ctx->song_length) {
            ctx->finished_render = 1;
        }
        if (written < frames) {
            break;
        }
    }
}

static void float_to_pcm16(const float *src, ALshort *dst, size_t sample_count) {
    if (src == NULL || dst == NULL || sample_count == 0u) {
        return;
    }
    for (size_t i = 0; i < sample_count; ++i) {
        float value = src[i];
        if (value > 1.0f) {
            value = 1.0f;
        } else if (value < -1.0f) {
            value = -1.0f;
        }
        dst[i] = (ALshort)(value * 32760.0f);
    }
}

static size_t queue_from_ring(ALuint buffer_id,
                              StreamContext *ctx,
                              float *mix_chunk,
                              ALshort *pcm_chunk,
                              size_t max_frames,
                              const SynthEngine *engine,
                              ALenum format) {
    if (ctx == NULL || mix_chunk == NULL || pcm_chunk == NULL || engine == NULL || max_frames == 0u) {
        return 0u;
    }

    const size_t frames = audio_ring_buffer_read(&ctx->ring, mix_chunk, max_frames);
    if (frames == 0u) {
        return 0u;
    }

    const unsigned int channels = engine->channels == 1 ? 1u : 2u;
    const size_t samples = frames * channels;
    float_to_pcm16(mix_chunk, pcm_chunk, samples);

    alBufferData(buffer_id,
                 format,
                 pcm_chunk,
                 (ALsizei)(samples * sizeof(ALshort)),
                 (ALsizei)engine->sample_rate);
    return frames;
}

static int check_al(const char *stage) {
    const ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        fprintf(stderr, "OpenAL error after %s: 0x%x\n", stage, err);
        return 0;
    }
    return 1;
}

int main(void) {
    const SynthEngine engine = {
        .sample_rate = 44100u,
        .channels = 2u,
    };

    const unsigned int channels = engine.channels == 1 ? 1u : 2u;
    const ALenum format = (channels == 1u) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    StreamContext stream_ctx = {
        .render_cursor = 0.0f,
        .song_length = synth_song_estimate_length(&DEMO_SONG),
        .finished_render = 0,
    };

    float *mix_chunk = NULL;
    ALshort *pcm_chunk = NULL;
    ALCdevice *device = NULL;
    ALCcontext *context = NULL;
    ALuint buffer_ids[STREAM_BUFFER_COUNT] = {0};
    ALuint source_id = 0;
    int buffers_created = 0;
    int source_created = 0;
    int exit_code = EXIT_SUCCESS;

    if (!audio_ring_buffer_init(&stream_ctx.ring, RING_BUFFER_FRAMES, channels)) {
        fprintf(stderr, "failed to allocate audio ring buffer\n");
        return EXIT_FAILURE;
    }

    const size_t scratch_samples = STREAM_CHUNK_FRAMES * channels;
    mix_chunk = malloc(scratch_samples * sizeof(float));
    pcm_chunk = malloc(scratch_samples * sizeof(ALshort));
    if (mix_chunk == NULL || pcm_chunk == NULL) {
        fprintf(stderr, "failed to allocate streaming buffers\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    refill_ring(&stream_ctx, &engine, &DEMO_SONG, mix_chunk, STREAM_CHUNK_FRAMES);

    device = alcOpenDevice(NULL);
    if (device == NULL) {
        fprintf(stderr, "failed to open OpenAL device\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    context = alcCreateContext(device, NULL);
    if (context == NULL || alcMakeContextCurrent(context) == ALC_FALSE) {
        fprintf(stderr, "failed to create OpenAL context\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    alGenBuffers(STREAM_BUFFER_COUNT, buffer_ids);
    if (!check_al("alGenBuffers")) {
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }
    buffers_created = 1;

    size_t initial_buffers = 0;
    for (size_t i = 0; i < STREAM_BUFFER_COUNT; ++i) {
        const size_t frames = queue_from_ring(buffer_ids[i],
                                              &stream_ctx,
                                              mix_chunk,
                                              pcm_chunk,
                                              STREAM_CHUNK_FRAMES,
                                              &engine,
                                              format);
        if (frames == 0u) {
            break;
        }
        ++initial_buffers;
    }

    if (initial_buffers == 0u) {
        fprintf(stderr, "no audio data available for streaming\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    alGenSources(1, &source_id);
    if (!check_al("alGenSources")) {
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }
    source_created = 1;

    alSourceQueueBuffers(source_id, (ALsizei)initial_buffers, buffer_ids);
    alSourcef(source_id, AL_GAIN, 0.9f);
    alSourcePlay(source_id);
    if (!check_al("alSourcePlay")) {
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    bool playback_done = false;
    while (!playback_done) {
        refill_ring(&stream_ctx, &engine, &DEMO_SONG, mix_chunk, STREAM_CHUNK_FRAMES);

        ALint processed = 0;
        alGetSourcei(source_id, AL_BUFFERS_PROCESSED, &processed);
        while (processed-- > 0) {
            ALuint buffer_id = 0;
            alSourceUnqueueBuffers(source_id, 1, &buffer_id);
            const size_t frames = queue_from_ring(buffer_id,
                                                  &stream_ctx,
                                                  mix_chunk,
                                                  pcm_chunk,
                                                  STREAM_CHUNK_FRAMES,
                                                  &engine,
                                                  format);
            if (frames > 0u) {
                alSourceQueueBuffers(source_id, 1, &buffer_id);
            }
        }

        ALint state = 0;
        alGetSourcei(source_id, AL_SOURCE_STATE, &state);
        ALint queued = 0;
        alGetSourcei(source_id, AL_BUFFERS_QUEUED, &queued);

        if (state != AL_PLAYING && queued > 0) {
            alSourcePlay(source_id);
        }

        if (stream_ctx.finished_render &&
            audio_ring_buffer_size(&stream_ctx.ring) == 0u &&
            queued == 0) {
            if (state != AL_PLAYING) {
                playback_done = true;
            }
        }

        sleep_millis(8);
    }

cleanup:
    if (source_created) {
        alSourceStop(source_id);
        ALint queued = 0;
        alGetSourcei(source_id, AL_BUFFERS_QUEUED, &queued);
        while (queued-- > 0) {
            ALuint buffer_id = 0;
            alSourceUnqueueBuffers(source_id, 1, &buffer_id);
        }
        alDeleteSources(1, &source_id);
    }

    if (buffers_created) {
        alDeleteBuffers(STREAM_BUFFER_COUNT, buffer_ids);
    }

    if (context != NULL) {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(context);
    }
    if (device != NULL) {
        alcCloseDevice(device);
    }

    audio_ring_buffer_free(&stream_ctx.ring);
    free(mix_chunk);
    free(pcm_chunk);

    return exit_code;
}
