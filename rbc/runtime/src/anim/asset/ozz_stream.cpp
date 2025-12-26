#include "rbc_anim/asset/ozz_stream.h"

namespace rbc {

OzzStream::OzzStream(rbc::ArchiveRead *_reader, rbc::ArchiveWrite *_writer) : reader_(_reader), writer_(_writer) {
    LUISA_ASSERT(reader_ || writer_);
    if (reader_)
        LUISA_ASSERT(!reader_->is_structured() && "streamed binary reader cannot be structured");
    if (writer_)
        LUISA_ASSERT(!writer_->is_structured() && "streamed binary writer cannot be structured");
}

OzzStream::~OzzStream() {
    LUISA_ASSERT(reader_ == nullptr && writer_ == nullptr);
}
bool OzzStream::opened() const {
    return reader_ != nullptr || writer_ != nullptr; 
}
size_t OzzStream::Read(void *_buffer, size_t _size) {
    LUISA_ASSERT(reader_);
    if (!reader_->bytes({(std::byte*)_buffer, _size}))
    {
        return 0;
    }
    return _size;
}
size_t OzzStream::Write(const void *_buffer, size_t _size) {
    LUISA_ASSERT(writer_);
    if (!writer_->bytes({(std::byte*)_buffer, _size}))
{
        return 0;
    }
    return _size;
}
int OzzStream::Seek(int _offset, Origin _origin) {
    RBC_UNIMPLEMENTED();
    return 0;
}
int OzzStream::Tell() const {
    RBC_UNIMPLEMENTED();
    return 0;
}
size_t OzzStream::Size() const {
    RBC_UNIMPLEMENTED();
    return 0;
}
}// namespace rbc