#include <rbc_graphics/bindless_allocator.h>
#include <luisa/core/logging.h>
#include <luisa/core/stl/algorithm.h>
namespace rbc
{
string_view BindlessAllocator::_name(uint index) const
{
    switch (index)
    {
    case 0:
        return "\"buffer\""sv;
    case 1:
        return "\"image\""sv;
    case 2:
        return "\"volume\""sv;
    default:
        return {};
    }
}
uint BindlessAllocator::_allocate_buffer(uint64 handle)
{
    uint v;
    {
        std::lock_guard lck{ _mtx };
        auto& vec = _vec[0];
        if (vec.empty()) [[unlikely]]
        {
            LUISA_ERROR("Allocator {} empty.", _name(0));
        }
        v = vec.back();
        vec.pop_back();
#ifndef NDEBUG
        if (!_allocated_map[0].try_emplace(v).second)
        {
            LUISA_ERROR("Allocator {} internal error, possibly multi-thread?", _name(0));
        }
#endif
    }
    _buffer_heap.emplace_buffer_handle_on_update(v, handle, 0);
    return v;
}
uint BindlessAllocator::_allocate_tex2d(uint64 handle, Sampler sampler)
{
    uint v;
    {
        std::lock_guard lck{ _mtx };
        auto& vec = _vec[1];
        if (vec.empty()) [[unlikely]]
        {
            LUISA_ERROR("Allocator {} empty.", _name(1));
        }
        v = vec.back();
        vec.pop_back();
#ifndef NDEBUG
        if (!_allocated_map[1].try_emplace(v).second)
        {
            LUISA_ERROR("Allocator {} internal error, possibly multi-thread?", _name(1));
        }
#endif
    }
    _image_heap.emplace_tex2d_handle_on_update(v, handle, sampler);
    return v;
}
uint BindlessAllocator::_allocate_tex3d(uint64 handle, Sampler sampler)
{
    uint v;
    {
        std::lock_guard lck{ _mtx };
        auto& vec = _vec[2];
        if (vec.empty()) [[unlikely]]
        {
            LUISA_ERROR("Allocator {} empty.", _name(2));
        }
        v = vec.back();
        vec.pop_back();
#ifndef NDEBUG
        if (!_allocated_map[2].try_emplace(v).second)
        {
            LUISA_ERROR("Allocator {} internal error, possibly multi-thread?", _name(2));
        }
#endif
    }
    _volume_heap.emplace_tex3d_handle_on_update(v, handle, sampler);
    return v;
}
void BindlessAllocator::_deallocate(uint index, uint value)
{
    std::lock_guard lck{ _mtx };
#ifndef NDEBUG
    auto iter = _allocated_map[index].find(value);
    if (!iter)
    {
        LUISA_ERROR("Invalid {} list free value {}", _name(index), value);
    }
    _allocated_map[index].remove(iter);
#endif
    _free_queue[index].emplace_back(value);
    switch (index)
    {
    case 0:
        _buffer_heap.remove_buffer_on_update(value);
        break;
    case 1:
        _image_heap.remove_tex2d_on_update(value);
        break;
    case 2:
        _volume_heap.remove_tex3d_on_update(value);
        break;
    }
}

void BindlessAllocator::commit(CommandList& cmdlist)
{
    _require_sync = false;
    if (_buffer_heap && _buffer_heap.dirty())
    {
        cmdlist << _buffer_heap.update();
    }
    if (_image_heap && _image_heap.dirty())
    {
        cmdlist << _image_heap.update();
    }
    if (_volume_heap && _volume_heap.dirty())
    {
        cmdlist << _volume_heap.update();
    }
    std::lock_guard lck{ _mtx };
    if (!(_free_queue[0].empty() && _free_queue[1].empty() && _free_queue[2].empty()))
    {
        cmdlist.add_callback([this, free_queue = std::move(_free_queue)]() {
            {
                std::lock_guard lck{ _mtx };
                for (auto i : vstd::range(3))
                {
                    vstd::push_back_all(_vec[i], luisa::span{ free_queue[i] });
                }
            }
        });
    }
}

BindlessAllocator::BindlessAllocator(
    Device& device,
    uint buffer_size,
    uint image_size,
    uint volume_size,
    vector<uint>&& reserved
)
{
    if (buffer_size > 0)
    {
        _buffer_heap = device.create_bindless_array(buffer_size, BindlessSlotType::BUFFER_ONLY);
    }
    if (image_size > 0)
    {
        _image_heap = device.create_bindless_array(image_size, BindlessSlotType::TEXTURE2D_ONLY);
    }
    if (volume_size > 0)
    {
        _volume_heap = device.create_bindless_array(volume_size, BindlessSlotType::TEXTURE3D_ONLY);
    }
    uint sizes[] = { buffer_size, image_size, volume_size };
    for (auto i : vstd::range(3))
    {
        auto size = sizes[i];
        if (size <= 0) continue;
        uint reserved_idx = 0;
        if (i < reserved.size())
        {
            reserved_idx = reserved[i];
        }
        LUISA_ASSERT(reserved_idx < size);
        auto last = size - 1;
        vstd::push_back_func(_vec[i], size - reserved_idx, [last, reserved_idx](auto i) { return last - i; });
    }
    for (auto i = 0; i < 3; ++i)
    {
        if (reserved.size() <= i) break;
        auto& vec = _reserved_handles[i];
        vstd::push_back_all(vec, reserved[i], invalid_resource_handle);
    }
}

bool BindlessAllocator::_set_reserve(uint idx, uint64_t handle, uint type)
{
    auto& vec = _reserved_handles[type];
    if (vec.size() <= idx) [[unlikely]]
    {
        LUISA_ERROR("Set reserved bindless index {} out of range {}.", idx, vec.size());
    }
    if (vec[idx] == handle)
    {
        return false;
    }
    vec[idx] = handle;
    _require_sync = true;
    return true;
}

void BindlessAllocator::_set_reserved_buffer(uint idx, uint64 handle)
{
    if (!_set_reserve(idx, handle, 0)) return;
    if (handle != invalid_resource_handle)
        _buffer_heap.emplace_buffer_handle_on_update(idx, handle, 0);
    else
        _buffer_heap.remove_buffer_on_update(idx);
}
void BindlessAllocator::_set_reserved_tex2d(uint idx, uint64 handle, Sampler sampler)
{
    if (!_set_reserve(idx, handle, 1)) return;
    if (handle != invalid_resource_handle)
        _image_heap.emplace_tex2d_handle_on_update(idx, handle, sampler);
    else
        _image_heap.remove_tex2d_on_update(idx);
}
void BindlessAllocator::_set_reserved_tex3d(uint idx, uint64 handle, Sampler sampler)
{
    if (!_set_reserve(idx, handle, 2)) return;
    if (handle != invalid_resource_handle)
        _volume_heap.emplace_tex3d_handle_on_update(idx, handle, sampler);
    else
        _volume_heap.remove_tex3d_on_update(idx);
}

BindlessAllocator::~BindlessAllocator()
{
#ifndef NDEBUG
    if (!_no_warning)
    {
        size_t alloc_index = 0;
        for (auto& map : _allocated_map)
        {
            if (!map.empty())
            {
                LUISA_WARNING("Allocator {} has unreleased values", alloc_index);
            }
            for (auto&& i : map)
            {
                LUISA_WARNING("Unreleased: {}", i);
            }
            alloc_index++;
        }
    }
#endif
}
} // namespace rbc
