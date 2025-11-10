#define _POSIX_C_SOURCE 200809L

#include <AL/al.h>
#include <AL/alc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "synthrave/instrument.h"
#include "synthrave/synth.h"

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

static int check_al(const char *stage) {
    const ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        fprintf(stderr, "OpenAL error after %s: 0x%x\n", stage, err);
        return 0;
    }
    return 1;
}

static void wait_for_completion(ALuint source) {
    ALint state = AL_PLAYING;
    const struct timespec request = {
        .tv_sec = 0,
        .tv_nsec = 16 * 1000 * 1000,
    };
    while (state == AL_PLAYING) {
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        nanosleep(&request, NULL);
    }
}

int main(void) {
    const SynthEngine engine = {
        .sample_rate = 44100u,
        .channels = 2u,
    };

    size_t frame_count = synth_engine_frames_for_song(&engine, &DEMO_SONG);
    if (frame_count == 0) {
        frame_count = (size_t)(engine.sample_rate * DEMO_SONG.length_seconds);
    }

    const size_t sample_count = frame_count * engine.channels;
    float *mix_buffer = calloc(sample_count, sizeof(float));
    if (mix_buffer == NULL) {
        fprintf(stderr, "failed to allocate mix buffer\n");
        return EXIT_FAILURE;
    }

    synth_engine_render(&engine, &DEMO_SONG, mix_buffer, frame_count);

    ALshort *pcm_buffer = malloc(sample_count * sizeof(ALshort));
    if (pcm_buffer == NULL) {
        fprintf(stderr, "failed to allocate pcm buffer\n");
        free(mix_buffer);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < sample_count; ++i) {
        float value = mix_buffer[i];
        if (value > 1.0f) {
            value = 1.0f;
        } else if (value < -1.0f) {
            value = -1.0f;
        }
        pcm_buffer[i] = (ALshort)(value * 32760.0f);
    }

    ALCdevice *device = alcOpenDevice(NULL);
    if (device == NULL) {
        fprintf(stderr, "failed to open OpenAL device\n");
        free(mix_buffer);
        free(pcm_buffer);
        return EXIT_FAILURE;
    }

    ALCcontext *context = alcCreateContext(device, NULL);
    if (context == NULL || alcMakeContextCurrent(context) == ALC_FALSE) {
        fprintf(stderr, "failed to create OpenAL context\n");
        if (context != NULL) {
            alcDestroyContext(context);
        }
        alcCloseDevice(device);
        free(mix_buffer);
        free(pcm_buffer);
        return EXIT_FAILURE;
    }

    ALuint buffer_id = 0;
    ALuint source_id = 0;
    alGenBuffers(1, &buffer_id);
    if (!check_al("alGenBuffers")) {
        goto cleanup;
    }

    alBufferData(buffer_id,
                 AL_FORMAT_STEREO16,
                 pcm_buffer,
                 (ALsizei)(sample_count * sizeof(ALshort)),
                 (ALsizei)engine.sample_rate);
    if (!check_al("alBufferData")) {
        goto cleanup;
    }

    alGenSources(1, &source_id);
    if (!check_al("alGenSources")) {
        goto cleanup;
    }

    alSourcei(source_id, AL_BUFFER, (ALint)buffer_id);
    alSourcef(source_id, AL_GAIN, 0.9f);
    alSourcePlay(source_id);
    if (!check_al("alSourcePlay")) {
        goto cleanup;
    }

    wait_for_completion(source_id);

cleanup:
    alSourceStop(source_id);
    alDeleteSources(1, &source_id);
    alDeleteBuffers(1, &buffer_id);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);

    free(mix_buffer);
    free(pcm_buffer);

    return 0;
}
