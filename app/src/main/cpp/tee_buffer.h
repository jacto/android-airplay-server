#ifndef TEE_BUFFER_H
#define TEE_BUFFER_H

#include <atomic>
#include <cstdint>
#include <vector>

/*
 * SPSC lock-free ring of interleaved stereo int16 frames.
 *
 * Producer: oboe audio callback (onAudioReady) — NEVER blocks, NEVER allocates:
 * a full ring drops the incoming remainder (live tap; the consumer is the mixer
 * pump pulling every ~21ms, so sustained overflow only happens if the consumer
 * died — dropping keeps the tap at live edge when it resumes).
 * Consumer: JNI drain thread (Java AirPlayAudioSource.readPcm via mixer pull).
 *
 * Monotonic head/tail counters masked into a power-of-two storage: classic
 * Lamport queue, one-sided release/acquire ordering pairs per side.
 */
class TeeBuffer {
public:
    explicit TeeBuffer(size_t capacityFrames)
        : mMask(roundPow2(capacityFrames) - 1), mStorage((mMask + 1) * 2) {}

    // audio callback: copy frames in (non-blocking, drops on full)
    void write(const int16_t *stereoInterleaved, size_t frames) {
        const size_t tail = mTail.load(std::memory_order_relaxed);
        const size_t space = mMask + 1 - (tail - mHead.load(std::memory_order_acquire));
        if (frames > space) frames = space;
        for (size_t i = 0; i < frames; i++) {
            const size_t slot = (tail + i) & mMask;
            mStorage[slot * 2] = stereoInterleaved[i * 2];
            mStorage[slot * 2 + 1] = stereoInterleaved[i * 2 + 1];
        }
        mTail.store(tail + frames, std::memory_order_release);
    }

    // consumer: copy out up to maxFrames; returns frames copied
    size_t read(int16_t *dst, size_t maxFrames) {
        const size_t head = mHead.load(std::memory_order_relaxed);
        size_t avail = mTail.load(std::memory_order_acquire) - head;
        if (avail > maxFrames) avail = maxFrames;
        for (size_t i = 0; i < avail; i++) {
            const size_t slot = (head + i) & mMask;
            dst[i * 2] = mStorage[slot * 2];
            dst[i * 2 + 1] = mStorage[slot * 2 + 1];
        }
        mHead.store(head + avail, std::memory_order_release);
        return avail;
    }

    // discard backlog (engine restart: stale audio must not leak into the tap)
    void flush() {
        mHead.store(mTail.load(std::memory_order_acquire), std::memory_order_release);
    }

private:
    static size_t roundPow2(size_t v) {
        size_t p = 1;
        while (p < v) p <<= 1;
        return p;
    }

    const size_t mMask;
    std::vector<int16_t> mStorage;  // (mask+1) frames * 2 ch
    alignas(64) std::atomic<size_t> mHead{0};
    alignas(64) std::atomic<size_t> mTail{0};
};

#endif  // TEE_BUFFER_H
