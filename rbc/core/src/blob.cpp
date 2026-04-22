#include "rbc_core/blob.h"
#include "rbc_core/memory.h"

namespace rbc {

const char *kSimpleBlobName = "SimpleBlob";

struct SimpleBlob : public IBlob {
public:
    SimpleBlob(const uint8_t *data, uint64_t size, uint64_t alignment, bool move, const char *name) noexcept
        : _size(size),
          _alignment(alignment) {
        if (move) {
            _bytes = (uint8_t *)data;
        } else if (size) {
            _bytes = (uint8_t *)rbc_malloc_alignedN(_size, _alignment, name ? name : kSimpleBlobName);
            if (data)
                memcpy(_bytes, data, _size);
        }
    }
    ~SimpleBlob() noexcept override {
        if (_bytes) {
            rbc_free_alignedN(_bytes, _alignment, kSimpleBlobName);
        }
        _bytes = nullptr;
    }
    uint8_t *get_data() const noexcept override { return _bytes; }
    uint64_t get_size() const noexcept override { return _size; }


private:
    uint64_t _size = 0;
    uint64_t _alignment = 0;
    uint8_t *_bytes = nullptr;
};

BlobId IBlob::Create(const uint8_t *data, uint64_t size, bool move, const char *name) noexcept {
    return BlobId(rbc::RC<SimpleBlob>::New(data, size, alignof(uint8_t), move, name));
}
BlobId IBlob::CreateAligned(const uint8_t *data, uint64_t size, uint64_t alignment, bool move, const char *name) noexcept {
    return BlobId(rbc::RC<SimpleBlob>::New(data, size, alignment, move, name));
}

}// namespace rbc