#include "dstorage_config.h"
#if defined(_WIN32) && (!defined(RBC_DSTORAGE_FALLBACK))
    #include <luisa/core/logging.h>
    #include "dstorage.h"
    #include <rbc_io//dstorage_interface.h>
    #include <d3d12.h>
    #include <wrl/client.h>
    #include <luisa/vstl/common.h>
    #include <luisa/backends/ext/native_resource_ext.hpp>

namespace rbc
{
using Microsoft::WRL::ComPtr;
    #ifndef ThrowIfFailed
inline const char* d3d12_error_name(HRESULT hr)
{
    switch (hr)
    {
    case D3D12_ERROR_ADAPTER_NOT_FOUND:
        return "D3D12_ERROR_ADAPTER_NOT_FOUND";
    case D3D12_ERROR_DRIVER_VERSION_MISMATCH:
        return "D3D12_ERROR_DRIVER_VERSION_MISMATCH";
    case DXGI_ERROR_ACCESS_DENIED:
        return "DXGI_ERROR_ACCESS_DENIED";
    case DXGI_ERROR_ACCESS_LOST:
        return "DXGI_ERROR_ACCESS_LOST";
    case DXGI_ERROR_ALREADY_EXISTS:
        return "DXGI_ERROR_ALREADY_EXISTS";
    case DXGI_ERROR_CANNOT_PROTECT_CONTENT:
        return "DXGI_ERROR_CANNOT_PROTECT_CONTENT";
    case DXGI_ERROR_DEVICE_HUNG:
        return "DXGI_ERROR_DEVICE_HUNG";
    case DXGI_ERROR_DEVICE_REMOVED:
        return "DXGI_ERROR_DEVICE_REMOVED";
    case DXGI_ERROR_DEVICE_RESET:
        return "DXGI_ERROR_DEVICE_RESET";
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
        return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
    case DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE:
        return "DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE";
    case DXGI_ERROR_FRAME_STATISTICS_DISJOINT:
        return "DXGI_ERROR_FRAME_STATISTICS_DISJOINT";
    case DXGI_ERROR_INVALID_CALL:
        return "DXGI_ERROR_INVALID_CALL";
    case DXGI_ERROR_MORE_DATA:
        return "DXGI_ERROR_MORE_DATA";
    case DXGI_ERROR_NAME_ALREADY_EXISTS:
        return "DXGI_ERROR_NAME_ALREADY_EXISTS";
    case DXGI_ERROR_NONEXCLUSIVE:
        return "DXGI_ERROR_NONEXCLUSIVE";
    case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE:
        return "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE";
    case DXGI_ERROR_NOT_FOUND:
        return "DXGI_ERROR_NOT_FOUND";
    case DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED:
        return "DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED";
    case DXGI_ERROR_REMOTE_OUTOFMEMORY:
        return "DXGI_ERROR_REMOTE_OUTOFMEMORY";
    case DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE:
        return "DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE";
    case DXGI_ERROR_SDK_COMPONENT_MISSING:
        return "DXGI_ERROR_SDK_COMPONENT_MISSING";
    case DXGI_ERROR_SESSION_DISCONNECTED:
        return "DXGI_ERROR_SESSION_DISCONNECTED";
    case DXGI_ERROR_UNSUPPORTED:
        return "DXGI_ERROR_UNSUPPORTED";
    case DXGI_ERROR_WAIT_TIMEOUT:
        return "DXGI_ERROR_WAIT_TIMEOUT";
    case DXGI_ERROR_WAS_STILL_DRAWING:
        return "DXGI_ERROR_WAS_STILL_DRAWING";
    case E_FAIL:
        return "E_FAIL";
    case E_INVALIDARG:
        return "E_INVALIDARG";
    case E_OUTOFMEMORY:
        return "E_OUTOFMEMORY";
    case E_NOTIMPL:
        return "E_NOTIMPL";
    case S_FALSE:
        return "S_FALSE";
    case S_OK:
        return "S_OK";
    default:
        break;
    }
    return "Unknown error";
}
        #define ThrowIfFailed(x)                                                          \
            do                                                                            \
            {                                                                             \
                HRESULT hr_ = (x);                                                        \
                if (hr_ != S_OK) [[unlikely]]                                             \
                {                                                                         \
                    LUISA_ERROR_WITH_LOCATION("D3D12 call '{}' failed with "              \
                                              "error {} (code = {}).",                    \
                                              #x, d3d12_error_name(hr_), (long long)hr_); \
                    abort();                                                              \
                }                                                                         \
            } while (false)
    #endif

class DStorageModule
{
public:
    luisa::DynamicModule dstorage_core_module;
    luisa::DynamicModule dstorage_module;
    ComPtr<IDStorageFactory> factory;

    DStorageModule(
        luisa::filesystem::path const& runtime_dir,
        size_t staging_size,
        bool force_hdd
    )
        : dstorage_core_module{ DynamicModule::load(runtime_dir, "dstoragecore") }
        , dstorage_module{ DynamicModule::load(runtime_dir, "dstorage") }
    {
        HRESULT(WINAPI * DStorageGetFactory)
        (REFIID riid, _COM_Outptr_ void** ppv);
        if (!dstorage_module || !dstorage_core_module)
        {
            LUISA_WARNING("Direct-Storage DLL not found.");
            return;
        }
        HRESULT(WINAPI * DStorageSetConfiguration1)
        (DSTORAGE_CONFIGURATION1 const* configuration);
        DStorageSetConfiguration1 = dstorage_module.function<std::remove_pointer_t<decltype(DStorageSetConfiguration1)>>("DStorageSetConfiguration1");
        if (force_hdd)
        {
            DSTORAGE_CONFIGURATION1 cfg{
                .DisableBypassIO = true,
                .ForceFileBuffering = true
            };
            DStorageSetConfiguration1(&cfg);
        }
        else
        {
            DSTORAGE_CONFIGURATION1 cfg{};
            DStorageSetConfiguration1(&cfg);
        }

        DStorageGetFactory = dstorage_module.function<std::remove_pointer_t<decltype(DStorageGetFactory)>>("DStorageGetFactory");
        DStorageGetFactory(IID_PPV_ARGS(factory.GetAddressOf()));
        factory->SetStagingBufferSize(staging_size);
    }
};
namespace dstorage_detail
{
static vstd::optional<DStorageModule> module;
static std::mutex module_mtx;
} // namespace dstorage_detail
struct DStorageStreamDX12Impl : DStorageStream
{
public:
    ~DStorageStreamDX12Impl();
    void enqueue_request(
        IOFile::Handle const& file,
        size_t offset_bytes,
        void* ptr,
        size_t len
    );
    void enqueue_request(
        IOFile::Handle const& file,
        size_t offset_bytes,
        uint64_t buffer_handle,
        void* buffer_ptr,
        size_t buffer_offset,
        size_t len
    );
    void enqueue_request(
        IOFile::Handle const& file,
        size_t offset_bytes,
        uint64_t tex_handle,
        void* tex_ptr,
        PixelStorage storage,
        uint3 offset,
        uint3 size,
        uint level
    );
    void enqueue_request(
        void const* mem_ptr,
        size_t offset_bytes,
        uint64_t buffer_handle,
        void* buffer_ptr,
        size_t buffer_offset,
        size_t len
    );
    void enqueue_request(
        void const* mem_ptr,
        size_t offset_bytes,
        void* ptr,
        size_t len
    );
    void enqueue_request(
        void const* mem_ptr,
        size_t offset_bytes,
        uint64_t tex_handle,
        void* tex_ptr,
        PixelStorage storage,
        uint3 offset,
        uint3 size,
        uint level
    );
    void enqueue_signal(
        uint64_t event_handle,
        void* event,
        uint64_t fence_index
    );
    void submit();
    void free_queue();
    bool is_event_complete(
        DeviceInterface* device_interface,
        uint64_t event_handle,
        void* evt_native_handle,
        uint64_t fence
    );
    void sync_event(
        DeviceInterface* device_interface,
        uint64_t event_handle,
        void* evt_native_handle, uint64_t fence
    );
    void init(
        luisa::filesystem::path const& runtime_dir,
        size_t staging_size,
        bool force_hdd
    );
    void dispose()
    {
        delete this;
    }
    bool timeline_signaled(uint64_t timeline) const override {
        return true;
    }
    DStorageStreamDX12Impl(
        Device& device,
        DStorageSrcType src_type
    );
};
void DStorageStream::init_dx12(
    luisa::filesystem::path const& runtime_dir,
    size_t staging_size,
    bool force_hdd
)
{
    using namespace dstorage_detail;
    std::lock_guard lck{ module_mtx };
    if (!module.has_value())
        module.create(runtime_dir, staging_size, force_hdd);
}
void DStorageStream::dispose_dx12()
{
    using namespace dstorage_detail;
    std::lock_guard lck{ module_mtx };
    if (module.has_value())
        module.destroy();
}

DStorageStream* DStorageStream::create_dx12(Device& device, DStorageSrcType src_type)
{
    return new DStorageStreamDX12Impl(device, src_type);
}

DStorageStreamDX12Impl::DStorageStreamDX12Impl(
    Device& device,
    DStorageSrcType src_type
)
    : DStorageStream(src_type)
{
    using namespace dstorage_detail;
    if (!module.has_value()) [[unlikely]]
    {
        LUISA_ERROR("init required before module.");
    }
    DSTORAGE_QUEUE_DESC queue_desc{
        .SourceType = static_cast<DSTORAGE_REQUEST_SOURCE_TYPE>(src_type),
        .Capacity = DSTORAGE_MAX_QUEUE_CAPACITY,
        .Priority = DSTORAGE_PRIORITY_LOW,
        .Device = reinterpret_cast<ID3D12Device*>(device.impl()->native_handle())
    };
    ThrowIfFailed(module->factory->CreateQueue(&queue_desc, IID_PPV_ARGS(&reinterpret_cast<IDStorageQueue2*&>(queue))));
}

DStorageStreamDX12Impl::~DStorageStreamDX12Impl()
{
    if (queue)
    {
        reinterpret_cast<IDStorageQueue2*>(queue)->Release();
    }
}
IOFile::Handle IOFile::_init_dx12(luisa::string_view path)
{
    IOFile::Handle handle;
    using namespace dstorage_detail;
    if (!module.has_value()) [[unlikely]]
    {
        LUISA_ERROR("init required before module.");
    }
    luisa::vector<wchar_t> wstr;
    wstr.push_back_uninitialized(path.size() + 1);
    wstr[path.size()] = 0;
    for (size_t i = 0; i < path.size(); ++i)
    {
        wstr[i] = path[i];
    }
    IDStorageFile* file;
    if (module->factory->OpenFile(wstr.data(), IID_PPV_ARGS(&file)) != S_OK) [[unlikely]]
    {
        LUISA_ERROR("Open file {} failed.", luisa::to_string(path));
    }
    handle.file = file;
    size_t length;
    BY_HANDLE_FILE_INFORMATION info{};
    ThrowIfFailed(file->GetFileInformation(&info));
    if constexpr (sizeof(size_t) > sizeof(DWORD))
    {
        length = info.nFileSizeHigh;
        length <<= (sizeof(DWORD) * 8);
        length |= info.nFileSizeLow;
    }
    else
    {
        length = info.nFileSizeLow;
    }
    handle.len = length;
    return handle;
}

void IOFile::_dispose_dx12(IOFile::Handle& handle)
{
    if (handle.file)
    {
        reinterpret_cast<IDStorageFile*>(handle.file)->Release();
    }
}
void DStorageStreamDX12Impl::enqueue_request(
    IOFile::Handle const& file,
    size_t offset_bytes,
    void* ptr,
    size_t len
)
{
    LUISA_ASSERT(src_type == DStorageSrcType::File, "Source type and queue mismatch.");
    DSTORAGE_REQUEST request{};
    request.Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
    request.Source.File.Source = reinterpret_cast<IDStorageFile*>(file.file);
    request.Source.File.Offset = offset_bytes;
    if (offset_bytes >= file.len) [[unlikely]]
    {
        LUISA_ERROR("Invalid offset");
    }
    if (len > file.len - offset_bytes) [[unlikely]]
    {
        LUISA_ERROR("Size out of range.");
    }
    request.Source.File.Size = len;
    request.Destination.Memory.Buffer = ptr;
    request.Destination.Memory.Size = len;
    reinterpret_cast<IDStorageQueue2*>(queue)->EnqueueRequest(&request);
}

void DStorageStreamDX12Impl::enqueue_request(
    IOFile::Handle const& file,
    size_t offset_bytes,
    uint64_t buffer_handle,
    void* buffer_ptr,
    size_t buffer_offset,
    size_t len
)
{
    LUISA_ASSERT(src_type == DStorageSrcType::File, "Source type and queue mismatch.");
    DSTORAGE_REQUEST request{};
    request.Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_BUFFER;
    request.Source.File.Source = reinterpret_cast<IDStorageFile*>(file.file);
    request.Source.File.Offset = offset_bytes;
    if (offset_bytes >= file.len) [[unlikely]]
    {
        LUISA_ERROR("Invalid offset");
    }
    if (len > file.len - offset_bytes) [[unlikely]]
    {
        LUISA_ERROR("Size out of range.");
    }
    request.Source.File.Size = len;
    request.Destination.Buffer.Resource = reinterpret_cast<ID3D12Resource*>(buffer_ptr);
    request.Destination.Buffer.Offset = buffer_offset;
    request.Destination.Buffer.Size = len;
    reinterpret_cast<IDStorageQueue2*>(queue)->EnqueueRequest(&request);
}
void DStorageStreamDX12Impl::enqueue_request(
    IOFile::Handle const& file,
    size_t offset_bytes,
    uint64_t tex_handle,
    void* tex_ptr,
    PixelStorage storage,
    uint3 offset,
    uint3 size,
    uint level
)
{
    LUISA_ASSERT(src_type == DStorageSrcType::File, "Source type and queue mismatch.");
    DSTORAGE_REQUEST request{};
    request.Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION;
    request.Source.File.Source = reinterpret_cast<IDStorageFile*>(file.file);
    request.Source.File.Offset = offset_bytes;
    if (offset_bytes >= file.len) [[unlikely]]
    {
        LUISA_ERROR("Invalid offset");
    }
    auto len = pixel_storage_size(storage, size);
    if (len > file.len - offset_bytes) [[unlikely]]
    {
        LUISA_ERROR("Size out of range.");
    }
    request.Source.File.Size = len;
    request.Destination.Texture.Resource = reinterpret_cast<ID3D12Resource*>(tex_ptr);
    request.Destination.Texture.SubresourceIndex = level;
    auto end = offset + size;
    request.Destination.Texture.Region = D3D12_BOX{
        offset.x,
        offset.y,
        offset.z,
        end.x,
        end.y,
        end.z
    };
    reinterpret_cast<IDStorageQueue2*>(queue)->EnqueueRequest(&request);
}
void DStorageStreamDX12Impl::enqueue_request(
    void const* mem_ptr,
    size_t offset_bytes,
    uint64_t buffer_handle,
    void* buffer_ptr,
    size_t buffer_offset,
    size_t len
)
{
    LUISA_ASSERT(src_type == DStorageSrcType::Memory, "Source type and queue mismatch.");
    DSTORAGE_REQUEST request{};
    request.Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_BUFFER;
    request.Source.Memory.Source = static_cast<std::byte const*>(mem_ptr) + offset_bytes;
    request.Source.Memory.Size = len;
    request.Destination.Buffer.Resource = reinterpret_cast<ID3D12Resource*>(buffer_ptr);
    request.Destination.Buffer.Offset = buffer_offset;
    request.Destination.Buffer.Size = len;
    reinterpret_cast<IDStorageQueue2*>(queue)->EnqueueRequest(&request);
}
void DStorageStreamDX12Impl::enqueue_request(
    void const* mem_ptr,
    size_t offset_bytes,
    uint64_t tex_handle,
    void* tex_ptr,
    PixelStorage storage,
    uint3 offset,
    uint3 size,
    uint level
)
{
    LUISA_ASSERT(src_type == DStorageSrcType::Memory, "Source type and queue mismatch.");
    DSTORAGE_REQUEST request{};
    request.Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION;
    auto len = pixel_storage_size(storage, size);
    request.Source.Memory.Source = static_cast<std::byte const*>(mem_ptr) + offset_bytes;
    request.Source.Memory.Size = len;
    request.Destination.Texture.Resource = reinterpret_cast<ID3D12Resource*>(tex_ptr);
    request.Destination.Texture.SubresourceIndex = level;
    auto end = offset + size;
    request.Destination.Texture.Region = D3D12_BOX{
        offset.x,
        offset.y,
        offset.z,
        end.x,
        end.y,
        end.z
    };
    reinterpret_cast<IDStorageQueue2*>(queue)->EnqueueRequest(&request);
}
void DStorageStreamDX12Impl::enqueue_request(
    void const* mem_ptr,
    size_t offset_bytes,
    void* ptr,
    size_t len
)
{
    LUISA_ASSERT(src_type == DStorageSrcType::Memory, "Source type and queue mismatch.");
    DSTORAGE_REQUEST request{};
    request.Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
    request.Source.Memory.Source = static_cast<std::byte const*>(mem_ptr) + offset_bytes;
    request.Source.Memory.Size = len;
    request.Destination.Memory.Buffer = ptr;
    request.Destination.Memory.Size = len;
    reinterpret_cast<IDStorageQueue2*>(queue)->EnqueueRequest(&request);
}
void DStorageStreamDX12Impl::enqueue_signal(
    uint64_t event_handle,
    void* event,
    uint64 fence_index
)
{
    reinterpret_cast<IDStorageQueue2*>(queue)->EnqueueSignal(reinterpret_cast<ID3D12Fence*>(event), fence_index);
}
void DStorageStreamDX12Impl::submit()
{
    reinterpret_cast<IDStorageQueue2*>(queue)->Submit();
}
void DStorageStreamDX12Impl::free_queue()
{
    if (queue)
    {
        reinterpret_cast<IDStorageQueue2*>(queue)->Release();
        queue = nullptr;
    }
}
bool DStorageStreamDX12Impl::is_event_complete(
    DeviceInterface* device_interface,
    uint64_t event_handle,
    void* evt, uint64_t idx
)
{
    auto fence = reinterpret_cast<ID3D12Fence*>(evt);
    return idx == 0 || fence->GetCompletedValue() >= idx;
}
void DStorageStreamDX12Impl::sync_event(
    DeviceInterface* device_interface,
    uint64_t event_handle,
    void* evt, uint64_t idx
)
{
    auto fence = reinterpret_cast<ID3D12Fence*>(evt);
    if (idx == 0) return;
    HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
    auto d = vstd::scope_exit([&] {
        CloseHandle(eventHandle);
    });
    if (fence->GetCompletedValue() < idx)
    {
        ThrowIfFailed(fence->SetEventOnCompletion(idx, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
    }
}
} // namespace rbc
#endif