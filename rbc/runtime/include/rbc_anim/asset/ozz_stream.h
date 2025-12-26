#pragma once
// specified serde stream for ozz
// This provides a streaming interface for ozz serialization that buffers data internally
// to work with structured archive formats like BinSerde.
#include "ozz/base/io/stream.h"
#include <luisa/core/stl/vector.h>
#include <rbc_config.h>

namespace rbc {

// OzzStream provides ozz::io::Stream interface with internal buffering.
// For writing: buffers all Write() calls, then flush via buffer() 
// For reading: takes data buffer, provides sequential Read() access
class RBC_RUNTIME_API OzzStream : public ozz::io::Stream {
public:
    // Construct for writing mode - creates empty internal buffer
    // After ozz serialization, call buffer() to get the data and write to archive
    OzzStream();

    // Construct for reading mode - takes data to read from
    // The data should be read from archive via bytes() before constructing OzzStream
    explicit OzzStream(luisa::span<const std::byte> data);

    virtual ~OzzStream();

    // ozz::io::Stream interface
    [[nodiscard]] bool opened() const override;
    [[nodiscard]] size_t Read(void *_buffer, size_t _size) override;
    [[nodiscard]] size_t Write(const void *_buffer, size_t _size) override;
    [[nodiscard]] int Seek(int _offset, Origin _origin) override;
    [[nodiscard]] int Tell() const override;
    [[nodiscard]] size_t Size() const override;

    // Get the buffered data after writing (for writing mode only)
    [[nodiscard]] luisa::span<const std::byte> buffer() const { return buffer_; }

    // Check if we're in write mode
    [[nodiscard]] bool is_write_mode() const { return is_write_mode_; }

private:
    luisa::vector<std::byte> buffer_;
    uint64_t pos_ = 0;
    bool is_write_mode_ = false;
};

}// namespace rbc
