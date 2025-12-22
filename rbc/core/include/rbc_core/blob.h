#pragma once

#include "rbc_core/rc.h"

namespace rbc {
struct IBlob;
using BlobId = RC<IBlob>;

struct RBC_CORE_API IBlob : public RCBase {
    static BlobId Create(const uint8_t *data, uint64_t size, bool move, const char *name = nullptr) noexcept;
    static BlobId CreateAligned(const uint8_t *data, uint64_t size, uint64_t alignment, bool move, const char *name = nullptr) noexcept;

    virtual ~IBlob() noexcept = default;
    virtual uint8_t *get_data() const noexcept = 0;
    virtual uint64_t get_size() const noexcept = 0;
};

}// namespace rbc