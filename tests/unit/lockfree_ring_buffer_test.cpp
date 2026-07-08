// tests/unit/lockfree_ring_buffer_test.cpp
//
// Unit tests for LockFreeAudioRingBuffer.

#include <concurrency/lockfree_ring_buffer.h>

#include <gtest/gtest.h>

#include <thread>
#include <vector>

using jyglobalvst::shared::LockFreeAudioRingBuffer;

TEST(LockFreeAudioRingBuffer, Construct)
{
    LockFreeAudioRingBuffer rb(256, 2);
    EXPECT_EQ(rb.capacity(), 255);  // power-of-two minus one wasted slot.
    EXPECT_EQ(rb.channels(), 2);
    EXPECT_EQ(rb.available(), 0);
}

TEST(LockFreeAudioRingBuffer, SingleWriteThenRead)
{
    LockFreeAudioRingBuffer rb(64, 2);
    float in_l[10] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    float in_r[10] = {11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f, 20.0f};
    const float* in[2] = {in_l, in_r};

    EXPECT_EQ(rb.tryWrite(in, 10), 10u);
    EXPECT_EQ(rb.available(), 10u);

    float out_l[10] = {};
    float out_r[10] = {};
    float* out[2] = {out_l, out_r};
    EXPECT_EQ(rb.tryRead(out, 10), 10u);
    EXPECT_EQ(rb.available(), 0u);

    for (std::size_t i = 0; i < 10; ++i)
    {
        EXPECT_FLOAT_EQ(out_l[i], in_l[i]);
        EXPECT_FLOAT_EQ(out_r[i], in_r[i]);
    }
}

TEST(LockFreeAudioRingBuffer, ReadReturnsAvailable)
{
    LockFreeAudioRingBuffer rb(64, 2);
    float in_l[10] = {};
    float in_r[10] = {};
    const float* in[2] = {in_l, in_r};
    (void)rb.tryWrite(in, 5);

    float out_l[10] = {};
    float out_r[10] = {};
    float* out[2] = {out_l, out_r};
    EXPECT_EQ(rb.tryRead(out, 10), 5u);
}

TEST(LockFreeAudioRingBuffer, WriteReturnsAvailable)
{
    LockFreeAudioRingBuffer rb(16, 2);  // capacity = 15
    float in_l[20] = {};
    float in_r[20] = {};
    const float* in[2] = {in_l, in_r};
    EXPECT_EQ(rb.tryWrite(in, 20), 15u);
    EXPECT_EQ(rb.available(), 15u);
}

TEST(LockFreeAudioRingBuffer, Wraparound)
{
    LockFreeAudioRingBuffer rb(16, 2);  // capacity = 15
    float in_l[16] = {};
    float in_r[16] = {};
    const float* in[2] = {in_l, in_r};
    float out_l[16] = {};
    float out_r[16] = {};
    float* out[2] = {out_l, out_r};

    // Fill buffer.
    (void)rb.tryWrite(in, 10);
    (void)rb.tryRead(out, 8);
    (void)rb.tryWrite(in, 10);

    EXPECT_EQ(rb.available(), 12u);
}

TEST(LockFreeAudioRingBuffer, ConcurrentProducerConsumer)
{
    constexpr std::size_t kFrames = 10000;
    constexpr std::size_t kBlock = 64;
    LockFreeAudioRingBuffer rb(512, 2);

    std::vector<float> produced_l(kFrames);
    std::vector<float> produced_r(kFrames);
    std::vector<float> consumed_l(kFrames);
    std::vector<float> consumed_r(kFrames);

    for (std::size_t i = 0; i < kFrames; ++i)
    {
        produced_l[i] = static_cast<float>(i);
        produced_r[i] = static_cast<float>(i + 100000);
    }

    std::thread producer([&]() {
        std::size_t written = 0;
        while (written < kFrames)
        {
            const std::size_t n = std::min(kBlock, kFrames - written);
            const float* in[2] = {produced_l.data() + written, produced_r.data() + written};
            const std::size_t w = rb.tryWrite(in, n);
            written += w;
        }
    });

    std::thread consumer([&]() {
        std::size_t read = 0;
        while (read < kFrames)
        {
            const std::size_t n = std::min(kBlock, kFrames - read);
            float* out[2] = {consumed_l.data() + read, consumed_r.data() + read};
            const std::size_t r = rb.tryRead(out, n);
            read += r;
        }
    });

    producer.join();
    consumer.join();

    for (std::size_t i = 0; i < kFrames; ++i)
    {
        EXPECT_FLOAT_EQ(consumed_l[i], produced_l[i]) << " at index " << i;
        EXPECT_FLOAT_EQ(consumed_r[i], produced_r[i]) << " at index " << i;
    }
}

TEST(LockFreeAudioRingBuffer, Clear)
{
    LockFreeAudioRingBuffer rb(64, 2);
    float in_l[10] = {1.0f};
    float in_r[10] = {2.0f};
    const float* in[2] = {in_l, in_r};
    (void)rb.tryWrite(in, 10);
    EXPECT_EQ(rb.available(), 10u);
    rb.clear();
    EXPECT_EQ(rb.available(), 0u);
}
