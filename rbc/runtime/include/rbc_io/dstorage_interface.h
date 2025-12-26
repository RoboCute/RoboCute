#pragma once
#include <rbc_config.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/rhi/device_interface.h>
namespace rbc {
enum struct DStorageSrcType : uint64_t {
    File = 0,
    Memory = 1
};

using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API IOFile {
    struct Handle {
    private:
        friend struct IOFile;
        friend struct DStorageStream;
        friend struct DStorageStreamDX12Impl;
        friend struct DStorageStreamFallbackImpl;
        void *file{nullptr};
        size_t len{0};

    public:
        explicit operator bool() const {
            return file != nullptr;
        }
        [[nodiscard]] size_t length() const { return len; }
    };

private:
    Handle _handle;
    static IOFile::Handle _init_dx12(luisa::string_view path);
    static IOFile::Handle _init_fallback(luisa::string_view path);
    static void _dispose_dx12(IOFile::Handle &);
    static void _dispose_fallback(IOFile::Handle &);

public:
    [[nodiscard]] auto const &handle() const { return _handle; }
    [[nodiscard]] auto file() const { return _handle.file; }
    [[nodiscard]] auto length() const { return _handle.len; }
    IOFile() = default;
    explicit IOFile(luisa::string_view path);
    IOFile(IOFile const &) = delete;
    IOFile(IOFile &&rhs) noexcept;
    explicit operator bool() const {
        return static_cast<bool>(_handle);
    }
    IOFile &operator=(IOFile const &) = delete;
    IOFile &operator=(IOFile &&rhs) noexcept {
        std::destroy_at(this);
        std::construct_at(this, std::move(rhs));
        return *this;
    }
    ~IOFile();
};

struct RBC_RUNTIME_API DStorageStream : RBCStruct {
protected:
    ~DStorageStream() = default;

public:
    void *queue{nullptr};
    DStorageSrcType src_type;
    explicit DStorageStream(DStorageSrcType src_type)
        : src_type(src_type) {
    }
    static void init_fallback();
    static DStorageStream *create_dx12(Device &device, DStorageSrcType src_type);
    static DStorageStream *create_fallback(Device &device, DStorageSrcType src_type);
    static void init_dx12(
        luisa::filesystem::path const &runtime_dir,
        bool force_hdd);
    static void dispose_dx12();
    virtual void dispose() = 0;
    DStorageStream(DStorageStream &&) = delete;
    DStorageStream(DStorageStream const &) = delete;
    virtual uint64_t staging_size() = 0;
    virtual bool support_wait() { return false; }
    // not support by all backend
    virtual void enqueue_wait(
        uint64_t event_handle,
        uint64_t fence_index) {}
    virtual void enqueue_request(
        IOFile::Handle const &file,
        size_t offset_bytes,
        void *ptr,
        size_t len) = 0;
    virtual void enqueue_request(
        IOFile::Handle const &file,
        size_t offset_bytes,
        uint64_t buffer_handle,
        void *buffer_ptr,
        size_t buffer_offset,
        size_t len) = 0;
    virtual void enqueue_request(
        IOFile::Handle const &file,
        size_t offset_bytes,
        uint64_t tex_handle,
        void *tex_ptr,
        PixelStorage storage,
        uint3 offset,
        uint3 size,
        uint level) = 0;
    virtual void enqueue_request(
        void const *mem_ptr,
        size_t offset_bytes,
        uint64_t buffer_handle,
        void *buffer_ptr,
        size_t buffer_offset,
        size_t len) = 0;
    virtual void enqueue_request(
        void const *mem_ptr,
        size_t offset_bytes,
        void *ptr,
        size_t len) = 0;
    virtual void enqueue_request(
        void const *mem_ptr,
        size_t offset_bytes,
        uint64_t tex_handle,
        void *tex_ptr,
        PixelStorage storage,
        uint3 offset,
        uint3 size,
        uint level) = 0;
    virtual void enqueue_signal(
        uint64_t event_handle,
        void *event,
        uint64_t fence_index) = 0;
    virtual void submit() = 0;
    virtual void free_queue() = 0;
    [[nodiscard]] virtual bool timeline_signaled(uint64_t timeline) const = 0;
    virtual void sync_event(
        DeviceInterface *device_interface,
        uint64_t event_handle,
        void *evt_native_handle, uint64_t fence) = 0;
    virtual bool is_event_complete(
        DeviceInterface *device_interface,
        uint64_t event_handle,
        void *evt_native_handle,
        uint64_t fence) = 0;
};
}// namespace rbc