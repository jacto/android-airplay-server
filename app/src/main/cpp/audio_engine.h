#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* drain the audio tee tap: up to maxFrames stereo-interleaved int16 frames copied into
 * dst (44100 Hz stereo, pre-volume). returns frames copied; 0 on nothing buffered or
 * transient rebuild contention. engine output continues unaffected (this is a copy). */
size_t audio_engine_read_tee(AudioEngine *engine, int16_t *dst, int maxFrames);

#ifdef __cplusplus
extern "C" {
#endif

/* native audio engine: decodes, buffers and outputs audio; config can change while active */
typedef struct AudioEngine AudioEngine;

/* native -> UI log channel, opaque here (C++ type) */
typedef struct LogSink LogSink;

/* report device's native output sample rate + frames-per-burst (from AudioManager) to
 * oboe. its OpenSL ES backend (pre-AAudio devices) can't discover these itself and
 * needs them to size low-latency buffers; unused by AAudio backend. global, call before
 * creating engine.
 * see: https://github.com/google/oboe/blob/main/docs/GettingStarted.md#obtaining-optimal-latency */
void audio_engine_set_default_stream_values(int sampleRate, int framesPerBurst);

/* change engine config; any thread */
bool audio_engine_configure(AudioEngine *engine, int cushionMs, int percentilePct,
                            int oboeBufferFrames, bool forceSwAlac, bool realtimePriority,
                            bool lowLatency, bool benchmarkLog);

/* output volume, linear gain */
void audio_engine_set_volume(AudioEngine *engine, float volume);

/* sender is switching audio formats */
void audio_engine_on_format(AudioEngine *engine, int ct, int spf);

/* begin or resume playout; false if stream fails to open; idempotent */
bool audio_engine_start(AudioEngine *engine);

/* pause playout, releases audio output devices while paused; idempotent */
void audio_engine_pause(AudioEngine *engine);

/* refresh + copy packed debug struct into dst; false if engine NULL or dstLen too small */
bool audio_engine_get_debug(AudioEngine *engine, void *dst, size_t dstLen);

/* stop + destroy engine; no concurrent or later audio_engine_decode calls allowed */
void audio_engine_destroy(AudioEngine *engine);

/* decode + play one encoded airplay packet; not concurrent with audio_engine_destroy */
void audio_engine_decode(AudioEngine *engine, const uint8_t *data, int len, int ct, int64_t pts_ns);

/* drain the audio tee tap: up to maxFrames stereo-interleaved int16 frames copied into
 * dst (44100 Hz stereo, pre-volume). returns frames copied; 0 on nothing buffered or
 * transient rebuild contention. engine output continues unaffected (this is a copy). */
size_t audio_engine_read_tee(AudioEngine *engine, int16_t *dst, int maxFrames);

#ifdef __cplusplus
}

#include <memory>

/* create engine; handle or NULL */
extern "C++" AudioEngine *audio_engine_create(std::shared_ptr<LogSink> log, int sampleRate,
                                              int channels);
#endif

#endif  // AUDIO_ENGINE_H
