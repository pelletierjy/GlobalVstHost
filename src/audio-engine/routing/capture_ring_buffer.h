#pragma once

#include "../../shared/concurrency/spsc_queue.h"
#include <cstdint>
#include <vector>

namespace jyglobalvst {

// REALTIME CONSTRAINTS: Ring buffer operations must be lock-free and non-blocking.
// Audio callback thread writes; VST callback thread reads.

template <typename T>
class CaptureRingBuffer {
public:
    explicit CaptureRingBuffer(size_t capacity_samples);
    
    // Write samples from WASAPI capture (audio callback thread)
    // Returns number of samples actually written
    size_t Write(const T* data, size_t num_samples);
    
    // Read samples for VST processing (VST callback thread)
    // Returns number of samples actually read
    size_t Read(T* out_data, size_t num_samples);
    
    // Get available samples to read
    size_t AvailableToRead() const;
    
    // Get space available to write
    size_t AvailableToWrite() const;
    
    // Check for underrun condition
    bool HasUnderrun() const { return underrun_count_ > 0; }
    uint64_t GetUnderrunCount() const { return underrun_count_; }
    
    // Reset state
    void Clear();
    
private:
    std::vector<T> buffer_;
    size_t write_pos_ = 0;
    size_t read_pos_ = 0;
    size_t capacity_ = 0;
    std::atomic<uint64_t> underrun_count_{0};
};

// Type aliases for common use
using FloatRingBuffer = CaptureRingBuffer<float>;
using Int16RingBuffer = CaptureRingBuffer<int16_t>;

}  // namespace jyglobalvst
