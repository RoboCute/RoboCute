#pragma once
// specified serde stream for ozz
#include "ozz/base/io/stream.h"
#include "rbc_core/serde.h"

namespace rbc {

class RBC_RUNTIME_API OzzStream : public ozz::io::Stream {
public:
    OzzStream(rbc::ArchiveRead *_reader, rbc::ArchiveWrite *_writer);

    virtual ~OzzStream();
    [[nodiscard]] bool opened() const override;
    [[nodiscard]] size_t Read(void *_buffer, size_t _size) override;
    [[nodiscard]] size_t Write(const void *_buffer, size_t _size) override;
    [[nodiscard]] int Seek(int _offset, Origin _origin) override;
    [[nodiscard]] int Tell() const override;
    [[nodiscard]] size_t Size() const override;

private:
    rbc::ArchiveRead *reader_;
    rbc::ArchiveWrite *writer_;
};

}// namespace rbc