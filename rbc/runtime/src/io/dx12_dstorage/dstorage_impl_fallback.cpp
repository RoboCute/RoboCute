#include "dstorage_config.h"
#include <luisa/core/logging.h>
#include <rbc_io//dstorage_interface.h>
#include <rbc_io//io_service.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/event.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/stack_allocator.h>
#include <luisa/backends/ext/pinned_memory_ext.hpp>
#ifdef _WIN32
#define RBC_FSEEK _fseeki64
#define RBC_FTELL _ftelli64
#else
#define RBC_FSEEK fseeko
#define RBC_FTELL ftello
#endif
namespace rbc {
struct DStorageStreamFallbackImpl : DStorageStream {
public:
    inline static uint64_t staging_size{};
    std::atomic_uint64_t signaled_fence_idx{};
    ~DStorageStreamFallbackImpl();
    void enqueue_request(
        IOFile::Handle const &file,
        size_t offset_bytes,
        void *ptr,
        size_t len);
    void enqueue_request(
        IOFile::Handle const &file,
        size_t offset_bytes,
        uint64_t buffer_handle,
        void *buffer_ptr,
        size_t buffer_offset,
        size_t len);
    void enqueue_request(
        IOFile::Handle const &file,
        size_t offset_bytes,
        uint64_t tex_handle,
        void *tex_ptr,
        PixelStorage storage,
        uint3 offset,
        uint3 size,
        uint level);
    void enqueue_request(
        void const *mem_ptr,
        size_t offset_bytes,
        uint64_t buffer_handle,
        void *buffer_ptr,
        size_t buffer_offset,
        size_t len);
    void enqueue_request(
        void const *mem_ptr,
        size_t offset_bytes,
        void *ptr,
        size_t len);
    void enqueue_request(
        void const *mem_ptr,
        size_t offset_bytes,
        uint64_t tex_handle,
        void *tex_ptr,
        PixelStorage storage,
        uint3 offset,
        uint3 size,
        uint level);
    void enqueue_signal(
        uint64_t event_handle,
        void *event,
        uint64_t fence_index);
    void submit();
    void free_queue();
    bool is_event_complete(
        DeviceInterface *device_interface,
        uint64_t event_handle,
        void *evt_native_handle,
        uint64_t fence);
    void sync_event(
        DeviceInterface *device_interface,
        uint64_t event_handle,
        void *evt_native_handle, uint64_t fence);
    void dispose() {
        delete this;
    }
    bool timeline_signaled(uint64_t timeline) const override {
        return signaled_fence_idx.load() >= timeline;
    }
    DStorageStreamFallbackImpl(
        Device &device,
        DStorageSrcType src_type);
};
DStorageStream *DStorageStream::create_fallback(Device &device, DStorageSrcType src_type) {
    return new DStorageStreamFallbackImpl(device, src_type);
}
namespace detail {
struct FrameBuffer {
    vstd::StackAllocator host_memory;
    Buffer<uint> upload_buffer;
    uint64_t buffer_offset{};
    FrameBuffer(uint64_t staging_size, PinnedMemoryExt *_pinnedmem_ext, vstd::VEngineMallocVisitor *visitor)
        : host_memory(staging_size, visitor), upload_buffer(
                                                  _pinnedmem_ext->allocate_pinned_memory<uint>(staging_size / sizeof(uint), PinnedMemoryOption{true})) {
    }

    void reset() {
        host_memory.clear();
        buffer_offset = 0;
    }
    uint64_t get_next_size(uint64_t size, uint64_t align) const {
        auto curr_offset = (buffer_offset + align - 1) & (~(align - 1));
        return curr_offset + size;
    }
    BufferView<uint> allocate(uint64_t size, uint64_t align) {
        buffer_offset = (buffer_offset + align - 1) & (~(align - 1));
        auto view = upload_buffer.view(buffer_offset / sizeof(uint), size / sizeof(uint));
        buffer_offset += size;
        return view;
    }
};
struct IOQueue : RBCStruct {
    Device *_device;
    PinnedMemoryExt *_pinnedmem_ext;
    uint64_t _staging_size;
    luisa::vector<luisa::move_only_function<void(IOQueue *)>> works;
    vstd::VEngineMallocVisitor visitor;
    vstd::unique_ptr<FrameBuffer> alloc;
    vstd::LockFreeArrayQueue<vstd::unique_ptr<FrameBuffer>> _waiting_allocs;
    luisa::spin_mutex work_thd;
    CommandList cmdlist;
    Stream stream;
    std::thread thd;
    std::atomic_uint64_t thd_fence_idx = 0;
    bool enabled = true;
    std::condition_variable cv;
    std::mutex mtx;

    IOQueue(Device &device, DStorageSrcType src_type, uint64_t staging_size)
        : _device(&device), _pinnedmem_ext(device.extension<PinnedMemoryExt>()), _staging_size(staging_size), thd([this, staging_size]() {
              uint64_t thd_wip_idx = 0;
              while (enabled) {
                  {
                      std::unique_lock lck{mtx};
                      while (thd_wip_idx >= thd_fence_idx.load()) {
                          if (!enabled) return;
                          cv.wait(lck);
                      }
                  }
                  while (thd_wip_idx < thd_fence_idx.load()) {
                      work_thd.lock();
                      auto curr_works = std::move(works);
                      work_thd.unlock();
                      for (auto &i : curr_works) {
                          i(this);
                      }
                      commit();
                      ++thd_wip_idx;
                  }
              }
          }) {
        stream = device.create_stream(StreamTag::COPY);
        alloc = vstd::make_unique<FrameBuffer>(staging_size, _pinnedmem_ext, &visitor);
    }
    void init_alloc() {
        if (alloc) return;
        auto v = _waiting_allocs.pop();
        if (v) {
            alloc = std::move(*v);
        } else {
            alloc = vstd::make_unique<FrameBuffer>(_staging_size, _pinnedmem_ext, &visitor);
        }
    }

    void commit() {
        if (!alloc) return;
        _pinnedmem_ext->flush_range(alloc->upload_buffer.handle(), 0, alloc->buffer_offset);
        if (!cmdlist.empty()) {
            cmdlist.add_callback([this, alloc = std::move(this->alloc)]() mutable {
                alloc->reset();
                _waiting_allocs.push(std::move(alloc));
            });
            stream << cmdlist.commit();
        } else {
            alloc->reset();
            _waiting_allocs.push(std::move(alloc));
        }
    }
    ~IOQueue() {
        {
            std::lock_guard lck{mtx};
            enabled = false;
        }
        cv.notify_one();
        thd.join();
        stream.synchronize();
    }
};
}// namespace detail
DStorageStreamFallbackImpl::DStorageStreamFallbackImpl(
    Device &device,
    DStorageSrcType src_type)
    : DStorageStream(src_type) {
    queue = new rbc::detail::IOQueue(device, src_type, staging_size);
}

DStorageStreamFallbackImpl::~DStorageStreamFallbackImpl() {
    if (queue) {
        delete static_cast<rbc::detail::IOQueue *>(queue);
    }
}
IOFile::Handle IOFile::_init_fallback(luisa::string_view path) {
    IOFile::Handle handle;
    luisa::string path_str{path};
    auto file = std::fopen(path_str.c_str(), "rb");
    RBC_FSEEK(file, 0, SEEK_END);
    auto length = RBC_FTELL(file);
    RBC_FSEEK(file, 0, SEEK_SET);
    handle.file = file;
    handle.len = length;
    return handle;
}
IOFile::IOFile(IOFile &&rhs) noexcept {
    _handle = rhs._handle;
    rhs._handle.file = nullptr;
    rhs._handle.len = 0;
}
IOFile::IOFile(luisa::string_view path) {
    switch (IOService::queue_type()) {
        case IOService::QueueType::DX12:
            _handle = IOFile::_init_dx12(path);
            break;
        default:
            _handle = IOFile::_init_fallback(path);
            break;
    }
}
IOFile::~IOFile() {
    switch (IOService::queue_type()) {
        case IOService::QueueType::DX12:
            IOFile::_dispose_dx12(_handle);
            break;
        default:
            IOFile::_dispose_fallback(_handle);
            break;
    }
}
void IOFile::_dispose_fallback(IOFile::Handle &handle) {
    if (handle.file)
        std::fclose(static_cast<::FILE *>(handle.file));
}
void DStorageStreamFallbackImpl::enqueue_request(
    IOFile::Handle const &file,
    size_t offset_bytes,
    void *ptr,
    size_t len) {
    LUISA_ASSERT(src_type == DStorageSrcType::File, "Source type and queue mismatch.");
    auto queue_impl = static_cast<rbc::detail::IOQueue *>(queue);
    luisa::move_only_function<void(rbc::detail::IOQueue *)> io_queue{[f = file.file, offset_bytes, ptr, len](rbc::detail::IOQueue *queue) {
        RBC_FSEEK(static_cast<::FILE *>(f), offset_bytes, SEEK_SET);
        fread(ptr, len, 1, static_cast<::FILE *>(f));
    }};
    queue_impl->work_thd.lock();
    queue_impl->works.emplace_back(std::move(io_queue));
    queue_impl->work_thd.unlock();
}

void DStorageStreamFallbackImpl::enqueue_request(
    IOFile::Handle const &file,
    size_t offset_bytes,
    uint64_t buffer_handle,
    void *buffer_ptr,
    size_t buffer_offset,
    size_t len) {
    LUISA_ASSERT(src_type == DStorageSrcType::File, "Source type and queue mismatch.");
    auto queue_impl = static_cast<rbc::detail::IOQueue *>(queue);
    luisa::move_only_function<void(rbc::detail::IOQueue *)> io_queue{[f = file.file, offset_bytes, buffer_handle, buffer_offset, len](rbc::detail::IOQueue *queue) {
        if (queue->alloc && queue->alloc->get_next_size(len, 256) > queue->alloc->upload_buffer.size_bytes()) {
            queue->commit();
        }
        queue->init_alloc();
        auto chunk = queue->alloc->host_memory.allocate(len, 256);
        auto ptr = reinterpret_cast<void *>(chunk.handle + chunk.offset);
        auto upload_buffer = queue->alloc->allocate(len, 256);
        auto upload_mapped_ptr = static_cast<std::byte *>(upload_buffer.native_handle()) + upload_buffer.offset_bytes();
        RBC_FSEEK(static_cast<::FILE *>(f), offset_bytes, SEEK_SET);
        fread(ptr, len, 1, static_cast<::FILE *>(f));
        std::memcpy(
            upload_mapped_ptr,
            ptr,
            len);
        queue->cmdlist << luisa::make_unique<BufferCopyCommand>(
            upload_buffer.handle(),
            buffer_handle,
            upload_buffer.offset_bytes(),
            buffer_offset,
            len);
    }};
    queue_impl->work_thd.lock();
    queue_impl->works.emplace_back(std::move(io_queue));
    queue_impl->work_thd.unlock();
}
void DStorageStreamFallbackImpl::enqueue_request(
    IOFile::Handle const &file,
    size_t offset_bytes,
    uint64_t tex_handle,
    void *tex_ptr,
    PixelStorage storage,
    uint3 offset,
    uint3 size,
    uint level) {
    LUISA_ASSERT(src_type == DStorageSrcType::File, "Source type and queue mismatch.");
    auto queue_impl = static_cast<rbc::detail::IOQueue *>(queue);
    luisa::move_only_function<void(rbc::detail::IOQueue *)> io_queue{
        [f = file.file, offset_bytes, tex_handle, storage, offset, size, level](rbc::detail::IOQueue *queue) {
            auto size_bytes = pixel_storage_size(storage, size);
            if (queue->alloc && queue->alloc->get_next_size(size_bytes, 256) > queue->alloc->upload_buffer.size_bytes()) {
                queue->commit();
            }
            queue->init_alloc();
            auto chunk = queue->alloc->host_memory.allocate(size_bytes, 256);
            auto ptr = reinterpret_cast<void *>(chunk.handle + chunk.offset);
            auto upload_buffer = queue->alloc->allocate(size_bytes, 256);
            auto upload_mapped_ptr = static_cast<std::byte *>(upload_buffer.native_handle()) + upload_buffer.offset_bytes();
            RBC_FSEEK(static_cast<::FILE *>(f), offset_bytes, SEEK_SET);
            fread(ptr, size_bytes, 1, static_cast<::FILE *>(f));
            std::memcpy(
                upload_mapped_ptr,
                ptr,
                size_bytes);
            queue->cmdlist << luisa::make_unique<BufferToTextureCopyCommand>(
                upload_buffer.handle(),
                upload_buffer.offset_bytes(),
                tex_handle,
                storage,
                level,
                size,
                offset);
        }};
    queue_impl->work_thd.lock();
    queue_impl->works.emplace_back(std::move(io_queue));
    queue_impl->work_thd.unlock();
}
void DStorageStreamFallbackImpl::enqueue_request(
    void const *mem_ptr,
    size_t offset_bytes,
    uint64_t buffer_handle,
    void *buffer_ptr,
    size_t buffer_offset,
    size_t len) {
    LUISA_ASSERT(src_type == DStorageSrcType::Memory, "Source type and queue mismatch.");
    auto queue_impl = static_cast<rbc::detail::IOQueue *>(queue);
    luisa::move_only_function<void(rbc::detail::IOQueue *)> io_queue{[mem_ptr, offset_bytes, buffer_handle, buffer_offset, len](rbc::detail::IOQueue *queue) {
        if (queue->alloc && queue->alloc->get_next_size(len, 256) > queue->alloc->upload_buffer.size_bytes()) {
            queue->commit();
        }
        queue->init_alloc();
        auto ptr = reinterpret_cast<std::byte const *>(mem_ptr) + offset_bytes;
        auto upload_buffer = queue->alloc->allocate(len, 256);
        auto upload_mapped_ptr = static_cast<std::byte *>(upload_buffer.native_handle()) + upload_buffer.offset_bytes();
        std::memcpy(
            upload_mapped_ptr,
            ptr,
            len);
        queue->cmdlist << luisa::make_unique<BufferCopyCommand>(
            upload_buffer.handle(),
            buffer_handle,
            upload_buffer.offset_bytes(),
            buffer_offset,
            len);
    }};
    queue_impl->work_thd.lock();
    queue_impl->works.emplace_back(std::move(io_queue));
    queue_impl->work_thd.unlock();
}
void DStorageStreamFallbackImpl::enqueue_request(
    void const *mem_ptr,
    size_t offset_bytes,
    uint64_t tex_handle,
    void *tex_ptr,
    PixelStorage storage,
    uint3 offset,
    uint3 size,
    uint level) {
    LUISA_ASSERT(src_type == DStorageSrcType::Memory, "Source type and queue mismatch.");
    auto queue_impl = static_cast<rbc::detail::IOQueue *>(queue);
    luisa::move_only_function<void(rbc::detail::IOQueue *)> io_queue{[mem_ptr, tex_handle, offset_bytes, storage, offset, size, level](rbc::detail::IOQueue *queue) {
        auto size_bytes = pixel_storage_size(storage, size);
        if (queue->alloc && queue->alloc->get_next_size(size_bytes, 256) > queue->alloc->upload_buffer.size_bytes()) {
            queue->commit();
        }
        queue->init_alloc();
        auto ptr = reinterpret_cast<std::byte const *>(mem_ptr) + offset_bytes;
        auto upload_buffer = queue->alloc->allocate(size_bytes, 256);
        std::memcpy(
            static_cast<std::byte *>(upload_buffer.native_handle()) + upload_buffer.offset_bytes(),
            ptr,
            size_bytes);
        queue->cmdlist << luisa::make_unique<BufferToTextureCopyCommand>(
            upload_buffer.handle(),
            upload_buffer.offset_bytes(),
            tex_handle,
            storage,
            level,
            size,
            offset);
    }};
    queue_impl->work_thd.lock();
    queue_impl->works.emplace_back(std::move(io_queue));
    queue_impl->work_thd.unlock();
}
void DStorageStreamFallbackImpl::enqueue_request(
    void const *mem_ptr,
    size_t offset_bytes,
    void *ptr,
    size_t len) {
    LUISA_ASSERT(src_type == DStorageSrcType::Memory, "Source type and queue mismatch.");
    auto queue_impl = static_cast<rbc::detail::IOQueue *>(queue);
    luisa::move_only_function<void(rbc::detail::IOQueue *)> io_queue{[mem_ptr, offset_bytes, ptr, len](rbc::detail::IOQueue *queue) {
        auto curr_ptr = reinterpret_cast<std::byte const *>(mem_ptr) + offset_bytes;
        std::memcpy(ptr, curr_ptr, len);
    }};
    queue_impl->work_thd.lock();
    queue_impl->works.emplace_back(std::move(io_queue));
    queue_impl->work_thd.unlock();
}
void DStorageStreamFallbackImpl::enqueue_signal(
    uint64_t event_handle,
    void *event,
    uint64 fence_index) {
    auto queue_impl = static_cast<rbc::detail::IOQueue *>(queue);
    luisa::move_only_function<void(rbc::detail::IOQueue *)> io_queue{[event_handle, fence_index, this](rbc::detail::IOQueue *queue) {
        queue->commit();
        queue->stream << Event::Signal{event_handle, fence_index};
        signaled_fence_idx = fence_index;
    }};
    queue_impl->work_thd.lock();
    queue_impl->works.emplace_back(std::move(io_queue));
    queue_impl->work_thd.unlock();
}
void DStorageStreamFallbackImpl::submit() {
    auto queue_impl = static_cast<rbc::detail::IOQueue *>(queue);
    {
        std::lock_guard lck{queue_impl->mtx};
        queue_impl->thd_fence_idx++;
    }
    queue_impl->cv.notify_one();
}
void DStorageStreamFallbackImpl::free_queue() {
    if (queue) {
        delete static_cast<rbc::detail::IOQueue *>(queue);
        queue = nullptr;
    }
}
bool DStorageStreamFallbackImpl::is_event_complete(
    DeviceInterface *device_interface,
    uint64_t event_handle,
    void *evt, uint64_t idx) {
    return idx == 0 || device_interface->is_event_completed(event_handle, idx);
}
void DStorageStreamFallbackImpl::sync_event(
    DeviceInterface *device_interface,
    uint64_t event_handle,
    void *evt, uint64_t idx) {
    device_interface->synchronize_event(event_handle, idx);
}

void DStorageStream::init_fallback(uint64_t staging_size) {
    DStorageStreamFallbackImpl::staging_size = staging_size;
}
}// namespace rbc
#undef RBC_FSEEK
#undef RBC_FTELL