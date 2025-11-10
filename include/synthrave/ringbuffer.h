#ifndef SYNTHRAVE_RINGBUFFER_H
#define SYNTHRAVE_RINGBUFFER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float *data;
    size_t capacity_frames;
    size_t channels;
    size_t head;
    size_t tail;
    size_t size;
} AudioRingBuffer;

int audio_ring_buffer_init(AudioRingBuffer *rb, size_t capacity_frames, size_t channels);
void audio_ring_buffer_free(AudioRingBuffer *rb);
void audio_ring_buffer_clear(AudioRingBuffer *rb);
size_t audio_ring_buffer_size(const AudioRingBuffer *rb);
size_t audio_ring_buffer_space(const AudioRingBuffer *rb);
size_t audio_ring_buffer_write(AudioRingBuffer *rb, const float *frames, size_t frame_count);
size_t audio_ring_buffer_read(AudioRingBuffer *rb, float *frames, size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHRAVE_RINGBUFFER_H */
