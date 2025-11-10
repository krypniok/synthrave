#include "ringbuffer.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int audio_ring_buffer_init(AudioRingBuffer *rb, size_t capacity_frames, size_t channels) {
    if (rb == NULL || capacity_frames == 0 || channels == 0) {
        return 0;
    }

    rb->data = calloc(capacity_frames * channels, sizeof(float));
    if (rb->data == NULL) {
        return 0;
    }

    rb->capacity_frames = capacity_frames;
    rb->channels = channels;
    rb->head = 0;
    rb->tail = 0;
    rb->size = 0;
    return 1;
}

void audio_ring_buffer_free(AudioRingBuffer *rb) {
    if (rb == NULL) {
        return;
    }
    free(rb->data);
    rb->data = NULL;
    rb->capacity_frames = 0;
    rb->channels = 0;
    rb->head = 0;
    rb->tail = 0;
    rb->size = 0;
}

void audio_ring_buffer_clear(AudioRingBuffer *rb) {
    if (rb == NULL) {
        return;
    }
    rb->head = 0;
    rb->tail = 0;
    rb->size = 0;
}

size_t audio_ring_buffer_size(const AudioRingBuffer *rb) {
    return rb ? rb->size : 0u;
}

size_t audio_ring_buffer_space(const AudioRingBuffer *rb) {
    if (rb == NULL) {
        return 0u;
    }
    return rb->capacity_frames - rb->size;
}

static size_t min_size(size_t a, size_t b) {
    return (a < b) ? a : b;
}

size_t audio_ring_buffer_write(AudioRingBuffer *rb, const float *frames, size_t frame_count) {
    if (rb == NULL || rb->data == NULL || frames == NULL || frame_count == 0) {
        return 0u;
    }

    const size_t writable = rb->capacity_frames - rb->size;
    const size_t to_write = min_size(frame_count, writable);
    if (to_write == 0u) {
        return 0u;
    }

    size_t remaining = to_write;
    size_t write_pos = rb->head;
    const size_t channels = rb->channels;
    const size_t capacity = rb->capacity_frames;
    const float *src = frames;

    while (remaining > 0u) {
        const size_t contiguous = min_size(remaining, capacity - write_pos);
        memcpy(rb->data + (write_pos * channels), src, contiguous * channels * sizeof(float));
        write_pos = (write_pos + contiguous) % capacity;
        src += contiguous * channels;
        remaining -= contiguous;
    }

    rb->head = write_pos;
    rb->size += to_write;
    return to_write;
}

size_t audio_ring_buffer_read(AudioRingBuffer *rb, float *frames, size_t frame_count) {
    if (rb == NULL || rb->data == NULL || frames == NULL || frame_count == 0) {
        return 0u;
    }

    const size_t readable = rb->size;
    const size_t to_read = min_size(frame_count, readable);
    if (to_read == 0u) {
        return 0u;
    }

    size_t remaining = to_read;
    size_t read_pos = rb->tail;
    const size_t channels = rb->channels;
    const size_t capacity = rb->capacity_frames;
    float *dst = frames;

    while (remaining > 0u) {
        const size_t contiguous = min_size(remaining, capacity - read_pos);
        memcpy(dst, rb->data + (read_pos * channels), contiguous * channels * sizeof(float));
        read_pos = (read_pos + contiguous) % capacity;
        dst += contiguous * channels;
        remaining -= contiguous;
    }

    rb->tail = read_pos;
    rb->size -= to_read;
    return to_read;
}
