#pragma once
#include <luisa/core/stl/memory.h>
#include <rbc_config.h>
#include <rbc_core/rc.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/binary_io.h>
#include <luisa/runtime/buffer.h>
namespace rbc {
struct DeviceResource : RCBase {
protected:
    std::atomic_uint64_t _gpu_load_frame{0};

public:
    struct FileLoad {
        luisa::filesystem::path path;
        size_t file_offset;
    };
    struct MemoryLoad {
        luisa::BinaryBlob blob;
    };
    struct BufferViewLoad {
        luisa::compute::BufferView<uint> buffer;
    };
    struct BufferLoad {
        luisa::compute::Buffer<uint> buffer;
    };

    enum struct Type {
        Buffer,
        Image,
        Mesh,
        TransformingMesh,
        SparseImage
    };
    virtual Type resource_type() const = 0;
    RBC_RUNTIME_API virtual bool load_finished() const;
    bool loaded() const {
        return _gpu_load_frame != 0;
    }
    virtual RBC_RUNTIME_API void sync_wait();
    uint64_t gpu_load_frame() const { return _gpu_load_frame.load(); }
    virtual ~DeviceResource() = default;
};
}// namespace rbc