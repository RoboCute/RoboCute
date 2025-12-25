#include "rbc_anim/asset/ozz_stream.h"

namespace rbc {

OzzStream::OzzStream(rbc::ArchiveRead *_reader, rbc::ArchiveWrite *_writer) : reader_(_reader), writer_(_writer) {
    LUISA_ASSERT(reader_ || writer_);
}

OzzStream::~OzzStream() {
    LUISA_ASSERT(reader_ == nullptr && writer_ == nullptr);
}
bool OzzStream::opened() const {
    // return reader_ != nullptr || writer_ != nullptr;
    return true;
}
size_t OzzStream::Read(void *_buffer, size_t _size) {
    // return reader_->read(_buffer, _size);
    return 0;
}
size_t OzzStream::Write(const void *_buffer, size_t _size) {
    // return writer_->write(_buffer, _size);
    return 0;
}
int OzzStream::Seek(int _offset, Origin _origin) {
    // return reader_->seek(_offset, _origin);
    return 0;
}
int OzzStream::Tell() const {
    // return reader_->tell();
    return 0;
}
size_t OzzStream::Size() const {
    // return reader_->size();
    return 0;
}
}// namespace rbc