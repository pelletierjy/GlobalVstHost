#include "capture_ring_buffer.h"
#include <algorithm>

namespace jyglobalvst {

template <typename T>
CaptureRingBuffer<T>::CaptureRingBuffer(size_t capacity_samples)
    : buffer_(capacity_samples), capacity_(capacity_samples) {}

template <typename T>
size_t CaptureRingBuffer<T>::Write(const T* data, size_t num_samples) {
    if (!data || num_samples == 0) {
        return 0;
    }
    
    size_t space = AvailableToWrite();
    size_t to_write = std::min(num_samples, space);
    
    if (to_write == 0) {
        underrun_count_.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    
    // Copy in two parts if wrap-around
    size_t part1 = std::min(to_write, capacity_ - write_pos_);
    std::copy(data, data + part1, buffer_.begin() + write_pos_);
    
    if (part1 < to_write) {
        size_t part2 = to_write - part1;
        std::copy(data + part1, data + part1 + part2, buffer_.begin());
    }
    
    write_pos_ = (write_pos_ + to_write) % capacity_;
    return to_write;
}

template <typename T>
size_t CaptureRingBuffer<T>::Read(T* out_data, size_t num_samples) {
    if (!out_data || num_samples == 0) {
        return 0;
    }
    
    size_t available = AvailableToRead();
    size_t to_read = std::min(num_samples, available);
    
    if (to_read == 0) {
        return 0;
    }
    
    // Copy in two parts if wrap-around
    size_t part1 = std::min(to_read, capacity_ - read_pos_);
    std::copy(buffer_.begin() + read_pos_, buffer_.begin() + read_pos_ + part1, out_data);
    
    if (part1 < to_read) {
        size_t part2 = to_read - part1;
        std::copy(buffer_.begin(), buffer_.begin() + part2, out_data + part1);
    }
    
    read_pos_ = (read_pos_ + to_read) % capacity_;
    return to_read;
}

template <typename T>
size_t CaptureRingBuffer<T>::AvailableToRead() const {
    size_t w = write_pos_;
    size_t r = read_pos_;
    
    if (w >= r) {
        return w - r;
    } else {
        return capacity_ - (r - w);
    }
}

template <typename T>
size_t CaptureRingBuffer<T>::AvailableToWrite() const {
    return capacity_ - AvailableToRead() - 1;  // -1 to distinguish full from empty
}

template <typename T>
void CaptureRingBuffer<T>::Clear() {
    write_pos_ = 0;
    read_pos_ = 0;
    underrun_count_.store(0, std::memory_order_release);
}

// Explicit instantiation
template class CaptureRingBuffer<float>;
template class CaptureRingBuffer<int16_t>;

}  // namespace jyglobalvst
