#pragma once
#include <rbc_config.h>
#include "bindless_allocator.h"
#include <luisa/vstl/common.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/sparse_image.h>
#include <luisa/runtime/sparse_volume.h>
#include <luisa/runtime/bindless_array.h>
namespace rbc
{
using namespace luisa::compute;
// API is thread safe
struct RBC_RUNTIME_API BindlessManager {
    enum struct ResourceType : uint64_t
    {
        Buffer,
        Tex2D,
        Tex3D
    };

private:
    struct Res {
        uint64_t res_handle;
        ResourceType type;
    };
    BindlessAllocator _alloc;
    vstd::HashMap<Res, uint> _map;
    uint _enqueue_buffer(uint64 handle);
    uint _enqueue_tex2d(uint64 handle, Sampler sampler);
    uint _enqueue_tex3d(uint64 handle, Sampler sampler);
    uint _index(uint64 handle, ResourceType type);
    uint _dequeue(uint64 handle, ResourceType type);
    luisa::spin_mutex _mtx;

public:
    void clear_all();
    [[nodiscard]] auto const& buffer_heap() const { return _alloc.buffer_heap(); }
    [[nodiscard]] auto& buffer_heap() { return _alloc.buffer_heap(); }
    [[nodiscard]] auto const& image_heap() const { return _alloc.image_heap(); }
    [[nodiscard]] auto& image_heap() { return _alloc.image_heap(); }
    [[nodiscard]] auto const& volume_heap() const { return _alloc.volume_heap(); }
    [[nodiscard]] auto& volume_heap() { return _alloc.volume_heap(); }
    [[nodiscard]] auto& alloc() { return _alloc; }
    template <typename T>
    uint enqueue_buffer(Buffer<T> const& buffer)
    {
        return _enqueue_buffer(buffer.handle());
    }
    uint enqueue_image(Image<float> const& image, Sampler sampler)
    {
        return _enqueue_tex2d(image.handle(), sampler);
    }
    uint enqueue_sparse_image(SparseImage<float> const& image, Sampler sampler)
    {
        return _enqueue_tex2d(image.handle(), sampler);
    }
    uint enqueue_volume(Volume<float> const& volume, Sampler sampler)
    {
        return _enqueue_tex3d(volume.handle(), sampler);
    }
    uint enqueue_sparse_volume(SparseVolume<float> const& volume, Sampler sampler)
    {
        return _enqueue_tex3d(volume.handle(), sampler);
    }
    uint index_of(ImageView<float> const& image)
    {
        return _index(image.handle(), ResourceType::Tex2D);
    }
    uint index_of(VolumeView<float> const& volume)
    {
        return _index(volume.handle(), ResourceType::Tex3D);
    }
    template <typename T>
    uint index_of(Buffer<T> const& buffer)
    {
        return _index(buffer.handle(), ResourceType::Buffer);
    }
    template <typename T>
    uint dequeue_buffer(Buffer<T> const& buffer)
    {
        return _dequeue(buffer.handle(), ResourceType::Buffer);
    }
    uint dequeue_image(Image<float> const& image)
    {
        return _dequeue(image.handle(), ResourceType::Tex2D);
    }
    uint dequeue_volume(Volume<float> const& volume)
    {
        return _dequeue(volume.handle(), ResourceType::Tex3D);
    }
    uint dequeue_sparse_image(SparseImage<float> const& image)
    {
        return _dequeue(image.handle(), ResourceType::Tex2D);
    }
    uint dequeue_sparse_volume(SparseVolume<float> const& volume)
    {
        return _dequeue(volume.handle(), ResourceType::Tex3D);
    }
    BindlessManager(Device& device);
    ~BindlessManager();
};
} // namespace rbc