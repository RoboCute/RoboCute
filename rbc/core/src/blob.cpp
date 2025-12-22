#include "rbc_core/blob.h"
#include "rbc_core/memory.h"

namespace rbc {

const char *kSimpleBlobName = "SimpleBlob";

struct SimpleBlob : public IBlob {
public:
    SimpleBlob(const uint8_t *data, uint64_t size, uint64_t alignment, bool move, const char *name) noexcept
        : size(size),
          alignment(alignment) {
        if (move) {
            bytes = (uint8_t *)data;
        } else if (size) {
            bytes = (uint8_t *)rbc_malloc_alignedN(size, alignment, name ? name : kSimpleBlobName);
            if (data)
                memcpy(bytes, data, size);
        }
    }
    ~SimpleBlob() noexcept {
        if (bytes) {
            rbc_free_alignedN(bytes, alignment, kSimpleBlobName);
        }
        bytes = nullptr;
    }
    uint8_t *get_data() const noexcept override { return bytes; }
    uint64_t get_size() const noexcept override { return size; }


private:
    uint64_t size = 0;
    uint64_t alignment = 0;
    uint8_t *bytes = nullptr;
};

BlobId IBlob::Create(const uint8_t *data, uint64_t size, bool move, const char *name) noexcept {
    return BlobId(rbc::RC<SimpleBlob>::New(data, size, alignof(uint8_t), move, name));
}
BlobId IBlob::CreateAligned(const uint8_t *data, uint64_t size, uint64_t alignment, bool move, const char *name) noexcept {
    return BlobId(rbc::RC<SimpleBlob>::New(data, size, alignment, move, name));
}

}// namespace rbc