#include <rbc_graphics/bindless_manager.h>
#include <luisa/core/logging.h>
namespace rbc
{
BindlessManager::BindlessManager(Device& device)
    : _alloc(device, 262144, 65536, 4096, []() {
        vector<uint> result = {
            heap_indices::BINDLESS_BUFFER_RESERVED_NUM,
            heap_indices::BINDLESS_TEX2D_RESERVED_NUM,
            heap_indices::BINDLESS_TEX3D_RESERVED_NUM
        };
        return result;
    }())
{
}

uint BindlessManager::_index(uint64 handle, ResourceType type)
{
    std::lock_guard lck{ _mtx };
    auto iter = _map.find(Res{ handle, type });
    LUISA_ASSERT(iter, "Resource invalid.");
    return iter.value();
}

uint BindlessManager::_enqueue_buffer(uint64 handle)
{

    std::lock_guard lck{ _mtx };
    auto iter = _map.try_emplace(Res{ handle, ResourceType::Buffer }, vstd::lazy_eval([&]() {
                                     return _alloc._allocate_buffer(handle);
                                 }));
    return iter.first.value();
}
uint BindlessManager::_enqueue_tex2d(uint64 handle, Sampler sampler)
{
    std::lock_guard lck{ _mtx };
    auto iter = _map.try_emplace(Res{ handle, ResourceType::Tex2D }, vstd::lazy_eval([&]() {
                                     return _alloc._allocate_tex2d(handle, sampler);
                                 }));
    return iter.first.value();
}
uint BindlessManager::_enqueue_tex3d(uint64 handle, Sampler sampler)
{
    std::lock_guard lck{ _mtx };
    auto iter = _map.try_emplace(Res{ handle, ResourceType::Tex3D }, vstd::lazy_eval([&]() {
                                     return _alloc._allocate_tex3d(handle, sampler);
                                 }));
    return iter.first.value();
}

uint BindlessManager::_dequeue(uint64 handle, ResourceType type)
{
    std::lock_guard lck{ _mtx };
    auto iter = _map.find(Res{ handle, type });
    LUISA_ASSERT(iter, "Invalid bindless dequeue");
    switch (iter.key().type)
    {
    case ResourceType::Buffer:
        _alloc.deallocate_buffer(iter.value());
        break;
    case ResourceType::Tex2D:
        _alloc.deallocate_tex2d(iter.value());
        break;
    case ResourceType::Tex3D:
        _alloc.deallocate_tex3d(iter.value());
        break;
    }
    auto v = iter.value();
    _map.remove(iter);
    return v;
}
void BindlessManager::clear_all()
{
    for (auto&& i : _map)
    {
        switch (i.first.type)
        {
        case ResourceType::Buffer:
            _alloc.deallocate_buffer(i.second);
            break;
        case ResourceType::Tex2D:
            _alloc.deallocate_tex2d(i.second);
            break;
        case ResourceType::Tex3D:
            _alloc.deallocate_tex3d(i.second);
            break;
        }
    }
    _map.clear();
}

BindlessManager::~BindlessManager()
{
}
} // namespace rbc