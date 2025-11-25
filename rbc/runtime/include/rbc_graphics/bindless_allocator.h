#pragma once
#include <rbc_config.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/spin_mutex.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/bindless_array.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/command_list.h>
namespace rbc
{
#include <utils/heap_indices.hpp>
using namespace luisa;
using namespace luisa::compute;
struct BindlessManager;
// API is thread safe
struct RBC_RUNTIME_API BindlessAllocator
{
private:
    friend struct BindlessManager;
    using Modification = BindlessArray::Modification;
    string_view _name(uint index) const;
    BindlessArray _buffer_heap;
    BindlessArray _image_heap;
    BindlessArray _volume_heap;
    vector<uint> _vec[3];
    vector<uint64_t> _reserved_handles[3];
    bool _require_sync{ false };
    std::array<vector<uint>, 3> _free_queue;
#ifndef NDEBUG
    vstd::HashMap<uint> _allocated_map[3];
    bool _no_warning = false;
#endif
    bool _set_reserve(uint idx, uint64_t handle, uint type);
    void _set_reserved_buffer(uint idx, uint64 handle);
    void _set_reserved_tex2d(uint idx, uint64 handle, Sampler sampler);
    void _set_reserved_tex3d(uint idx, uint64 handle, Sampler sampler);
    uint _allocate_buffer(uint64 handle);
    uint _allocate_tex2d(uint64 handle, Sampler sampler);
    uint _allocate_tex3d(uint64 handle, Sampler sampler);
    void _deallocate(uint index, uint value);
    luisa::spin_mutex _mtx;

public:
    [[nodiscard]] auto require_sync() const { return _require_sync; }
    // Do not use this only if required
    void _set_sync() { _require_sync = true; }

    void set_no_warning()
    {
#ifndef NDEBUG
        _no_warning = true;
#endif
    }
    BindlessAllocator(
        Device& device,
        uint buffer_size,
        uint image_size,
        uint volume_size,
        vector<uint>&& reserved
    );
    ~BindlessAllocator();
    template <typename T>
    void set_reserved_buffer(uint idx, Buffer<T> const& buffer)
    {
        _set_reserved_buffer(idx, buffer.handle());
    }
    void set_reserved_tex2d(uint idx, Image<float> const& img, Sampler sampler)
    {
        _set_reserved_tex2d(idx, img.handle(), sampler);
    }
    void set_reserved_tex2d(uint idx, SparseImage<float> const& img, Sampler sampler)
    {
        _set_reserved_tex2d(idx, img.handle(), sampler);
    }
    void set_reserved_tex3d(uint idx, Volume<float> const& volume, Sampler sampler)
    {
        _set_reserved_tex3d(idx, volume.handle(), sampler);
    }
    void set_reserved_tex3d(uint idx, SparseVolume<float> const& volume, Sampler sampler)
    {
        _set_reserved_tex3d(idx, volume.handle(), sampler);
    }
    void remove_reserved_buffer(uint idx)
    {
        _set_reserved_buffer(idx, invalid_resource_handle);
    }
    void remove_reserved_tex2d(uint idx)
    {
        _set_reserved_tex2d(idx, invalid_resource_handle, {});
    }
    void remove_reserved_tex3d(uint idx)
    {
        _set_reserved_tex3d(idx, invalid_resource_handle, {});
    }

    template <typename T>
    uint allocate_buffer(Buffer<T> const& buffer)
    {
        return _allocate_buffer(buffer.handle());
    }
    uint allocate_tex2d(Image<float> const& img, Sampler sampler)
    {
        return _allocate_tex2d(img.handle(), sampler);
    }
    uint allocate_tex2d(SparseImage<float> const& img, Sampler sampler)
    {
        return _allocate_tex2d(img.handle(), sampler);
    }
    uint allocate_tex3d(Volume<float> const& volume, Sampler sampler)
    {
        return _allocate_tex3d(volume.handle(), sampler);
    }
    uint allocate_tex3d(SparseVolume<float> const& volume, Sampler sampler)
    {
        return _allocate_tex3d(volume.handle(), sampler);
    }
    void deallocate_buffer(uint value)
    {
        _deallocate(0, value);
    }
    void deallocate_tex2d(uint value)
    {
        _deallocate(1, value);
    }
    void deallocate_tex3d(uint value)
    {
        _deallocate(2, value);
    }
    [[nodiscard]] auto const& buffer_heap() const { return _buffer_heap; }
    [[nodiscard]] auto& buffer_heap() { return _buffer_heap; }
    [[nodiscard]] auto const& image_heap() const { return _image_heap; }
    [[nodiscard]] auto& image_heap() { return _image_heap; }
    [[nodiscard]] auto const& volume_heap() const { return _volume_heap; }
    [[nodiscard]] auto& volume_heap() { return _volume_heap; }
    void commit(CommandList& cmdlist);
};
} // namespace rbc