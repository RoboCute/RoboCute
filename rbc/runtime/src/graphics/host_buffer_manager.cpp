#include <rbc_graphics/host_buffer_manager.h>
namespace rbc
{
void HostBufferManager::clear()
{
    _alloc.dispose();
    _dispose_list.clear();
}
HostBufferManager::HostBufferManager(Device& device)
    : _alloc(65536, this, 0)
    , _device(device)
{
    ext = _device.extension<PinnedMemoryExt>();
}
HostBufferManager::~HostBufferManager() {}
uint64 HostBufferManager::allocate(uint64 size)
{
    uint64 idx;
    if (_buffer_pool.empty())
    {
        idx = _buffers.size();
        _buffers.emplace_back(ext->allocate_pinned_memory<uint>(size / sizeof(uint), PinnedMemoryOption{ true }));
    }
    else
    {
        idx = _buffer_pool.back();
        _buffer_pool.pop_back();
        _buffers[idx] = ext->allocate_pinned_memory<uint>(size / sizeof(uint), PinnedMemoryOption{ true });
    }
    return idx;
}
BufferView<uint> HostBufferManager::_alloc_buffer(size_t byte_size, size_t align)
{
    auto r = _alloc.allocate(byte_size, align);
    return _buffers[r.handle].view(r.offset / sizeof(uint), byte_size / sizeof(uint));
}
void HostBufferManager::deallocate(uint64 handle)
{
    _dispose_list.emplace_back(std::move(_buffers[handle]));
    _buffer_pool.emplace_back(handle);
}
void HostBufferManager::flush()
{
    for (auto& i : _buffers)
    {
        ext->flush_range(i.handle(), 0, i.size_bytes());
    }
}

} // namespace rbc