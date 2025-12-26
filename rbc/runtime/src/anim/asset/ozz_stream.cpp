#include "rbc_anim/asset/ozz_stream.h"
#include <luisa/core/logging.h>
#include <cstring>

namespace rbc {

// Write mode constructor - creates empty buffer
OzzStream::OzzStream() : pos_(0), is_write_mode_(true) {
}

// Read mode constructor - takes data buffer
OzzStream::OzzStream(luisa::span<const std::byte> data) : pos_(0), is_write_mode_(false) {
    buffer_.resize(data.size());
    if (!data.empty()) {
        std::memcpy(buffer_.data(), data.data(), data.size());
    }
}

OzzStream::~OzzStream() {}

bool OzzStream::opened() const {
    return true;
}

size_t OzzStream::Read(void *_buffer, size_t _size) {
    LUISA_ASSERT(!is_write_mode_, "OzzStream: Cannot read in write mode");
    
    if (_size == 0) return 0;
    
    // Check if we have enough data
    if (pos_ + _size > buffer_.size()) {
        LUISA_WARNING("OzzStream: Read past end of buffer (pos={}, size={}, buffer_size={})", 
                      pos_, _size, buffer_.size());
        return 0;
    }
    
    std::memcpy(_buffer, buffer_.data() + pos_, _size);
    pos_ += _size;
    return _size;
}

size_t OzzStream::Write(const void *_buffer, size_t _size) {
    LUISA_ASSERT(is_write_mode_, "OzzStream: Cannot write in read mode");
    
    if (_size == 0) return 0;
    
    // Append to buffer
    auto old_size = buffer_.size();
    buffer_.resize(old_size + _size);
    std::memcpy(buffer_.data() + old_size, _buffer, _size);
    pos_ += _size;
    return _size;
}

int OzzStream::Seek(int _offset, Origin _origin) {
    int64_t new_pos;
    switch (_origin) {
        case kSet:
            new_pos = _offset;
            break;
        case kCurrent:
            new_pos = static_cast<int64_t>(pos_) + _offset;
            break;
        case kEnd:
            new_pos = static_cast<int64_t>(buffer_.size()) + _offset;
            break;
        default:
            return -1;
    }
    
    if (new_pos < 0 || new_pos > static_cast<int64_t>(buffer_.size())) {
        return -1;
    }
    
    pos_ = static_cast<uint64_t>(new_pos);
    return 0;
}

int OzzStream::Tell() const {
    return static_cast<int>(pos_);
}

size_t OzzStream::Size() const {
    return buffer_.size();
}

}// namespace rbc
