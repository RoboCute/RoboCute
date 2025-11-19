#pragma once
#include <rbc_config.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/rhi/device_interface.h>
namespace rbc
{
enum class DStorageSrcType : uint64_t
{
    File = 0,
    Memory = 1
};

using namespace luisa;
using namespace luisa::compute;
class RBC_RUNTIME_API IOFile
{
public:
    class Handle
    {
        friend class IOFile;
        friend class DStorageStream;
        friend class DStorageStreamDX12Impl;
        friend class DStorageStreamFallbackImpl;
        void* file{ nullptr };
        size_t len{ 0 };

    public:
        operator bool() const
        {
            return file != nullptr;
        }
        size_t length() const { return len; }
    };

private:
    Handle _handle;
    static IOFile::Handle _init_dx12(luisa::string_view path);
    static IOFile::Handle _init_fallback(luisa::string_view path);
    static void _dispose_dx12(IOFile::Handle&);
    static void _dispose_fallback(IOFile::Handle&);

public:
    auto const& handle() const { return _handle; }
    auto file() const { return _handle.file; }
    auto length() const { return _handle.len; }
    IOFile() = default;
    IOFile(luisa::string_view file);
    IOFile(IOFile const&) = delete;
    IOFile(IOFile&&);
    operator bool() const
    {
        return _handle;
    }
    IOFile& operator=(IOFile const&) = delete;
    IOFile& operator=(IOFile&& rhs)
    {
        this->~IOFile();
        new (std::launder(this)) IOFile{ std::move(rhs) };
        return *this;
    }
    ~IOFile();
};

class RBC_RUNTIME_API DStorageStream : public vstd::IOperatorNewBase
{
protected:
    ~DStorageStream() = default;

public:
    void* queue{ nullptr };
    DStorageSrcType src_type;
    DStorageStream(DStorageSrcType src_type)
        : src_type(src_type)
    {
    }
    static void init_fallback(uint64_t staging_size);
    static DStorageStream* create_dx12(Device& device, DStorageSrcType src_type);
    static DStorageStream* create_fallback(Device& device, DStorageSrcType src_type);
    static void init_dx12(
        luisa::filesystem::path const& runtime_dir,
        size_t staging_size,
        bool force_hdd
    );
    static void dispose_dx12();
    virtual void dispose() = 0;
    DStorageStream(DStorageStream&&) = delete;
    DStorageStream(DStorageStream const&) = delete;
    virtual void enqueue_request(
        IOFile::Handle const& file,
        size_t file_offset,
        void* ptr,
        size_t len
    ) = 0;
    virtual void enqueue_request(
        IOFile::Handle const& file,
        size_t file_offset,
        uint64_t buffer_handle,
        void* buffer_ptr,
        size_t buffer_offset,
        size_t len
    ) = 0;
    virtual void enqueue_request(
        IOFile::Handle const& file,
        size_t file_offset,
        uint64_t tex_handle,
        void* tex_ptr,
        PixelStorage storage,
        uint3 offset,
        uint3 size,
        uint level
    ) = 0;
    virtual void enqueue_request(
        void const* mem_ptr,
        size_t file_offset,
        uint64_t buffer_handle,
        void* buffer_ptr,
        size_t buffer_offset,
        size_t len
    ) = 0;
    virtual void enqueue_request(
        void const* mem_ptr,
        size_t file_offset,
        void* ptr,
        size_t len
    ) = 0;
    virtual void enqueue_request(
        void const* mem_ptr,
        size_t file_offset,
        uint64_t tex_handle,
        void* tex_ptr,
        PixelStorage storage,
        uint3 offset,
        uint3 size,
        uint level
    ) = 0;
    virtual void enqueue_signal(
        uint64_t event_handle,
        void* event,
        uint64_t fence_index
    ) = 0;
    virtual void submit() = 0;
    virtual void free_queue() = 0;
    virtual void sync_event(
        DeviceInterface* device_interface,
        uint64_t event_handle,
        void* evt_native_handle, uint64_t fence
    ) = 0;
    virtual bool is_event_complete(
        DeviceInterface* device_interface,
        uint64_t event_handle,
        void* evt_native_handle,
        uint64_t fence
    ) = 0;
};
} // namespace rbc