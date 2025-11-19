#include <rbc_graphics/make_device_config.h>
#include "dx_device_info.h"
#include <wrl/client.h>
#include <luisa/core/logging.h>
#ifndef ThrowIfFailed
    #define ThrowIfFailed(x)                                                                          \
        do                                                                                            \
        {                                                                                             \
            HRESULT hr_ = (x);                                                                        \
            if (hr_ != S_OK) [[unlikely]]                                                             \
            {                                                                                         \
                LUISA_ERROR_WITH_LOCATION("D3D12 call '{}' failed with "                              \
                                          "error {} (code = {}).",                                    \
                                          #x, rbc::dx_detail::d3d12_error_name(hr_), (long long)hr_); \
                std::abort();                                                                         \
            }                                                                                         \
        } while (false)
#endif
namespace rbc
{
namespace dx_detail
{
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
inline int ComputeIntersectionArea(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2)
{
    return std::max(0, std::min(ax2, bx2) - std::max(ax1, bx1)) * std::max(0, std::min(ay2, by2) - std::max(ay1, by1));
}
} // namespace dx_detail
using Microsoft::WRL::ComPtr;
RBC_RUNTIME_API luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_dx_device_config(
    void* device,
    bool gpu_dump
)
{
    auto ptr = luisa::make_unique<rbc::DXDeviceInfo>((ID3D12Device*)device);
    ptr->gpu_dump = gpu_dump;
    if (gpu_dump)
    {
        LUISA_WARNING("GPU Dump emabled.");
    }
    return ptr;
}

void DXDeviceInfo::ReadbackDX12Device(
    ID3D12Device* device,
    IDXGIAdapter1* adapter,
    IDXGIFactory2* factory,
    DirectXFuncTable const* funcTable,
    luisa::BinaryIO const* shaderIo,
    IDxcCompiler3* dxcCompiler,
    IDxcLibrary* dxcLibrary,
    IDxcUtils* dxcUtils,
    ID3D12DescriptorHeap* shaderDescriptor,
    ID3D12DescriptorHeap* samplerDescriptor
) noexcept
{
    this->device = device;
    this->adapter = adapter;
    this->factory = factory;
    this->funcTable = funcTable;
    this->shaderIo = shaderIo;
    this->dxcCompiler = dxcCompiler;
    this->dxcLibrary = dxcLibrary;
    this->dxcUtils = dxcUtils;
    this->shaderDescriptor = shaderDescriptor;
    this->samplerDescriptor = samplerDescriptor;
}

DXGI_OUTPUT_DESC1 DXDeviceInfo::GetOutput(HWND window_handle)
{
    /////// Get window bound
    RECT windowRect = {};
    GetWindowRect(window_handle, &windowRect);

    UINT i = 0;
    Microsoft::WRL::ComPtr<IDXGIOutput> currentOutput;
    Microsoft::WRL::ComPtr<IDXGIOutput> bestOutput;
    float bestIntersectArea = -1;

    while (adapter->EnumOutputs(i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
    {
        // Get the retangle bounds of the app window
        int ax1 = windowRect.left;
        int ay1 = windowRect.top;
        int ax2 = windowRect.right;
        int ay2 = windowRect.bottom;

        // Get the rectangle bounds of current output
        DXGI_OUTPUT_DESC desc;
        ThrowIfFailed(currentOutput->GetDesc(&desc));
        RECT r = desc.DesktopCoordinates;
        int bx1 = r.left;
        int by1 = r.top;
        int bx2 = r.right;
        int by2 = r.bottom;

        // Compute the intersection
        int intersectArea = dx_detail::ComputeIntersectionArea(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
        if (intersectArea > bestIntersectArea)
        {
            bestOutput = currentOutput;
            bestIntersectArea = static_cast<float>(intersectArea);
        }

        i++;
    }

    // Having determined the output (display) upon which the app is primarily being
    // rendered, retrieve the HDR capabilities of that display by checking the color space.
    ComPtr<IDXGIOutput6> output6;
    ThrowIfFailed(bestOutput.As(&output6));

    DXGI_OUTPUT_DESC1 desc1;
    ThrowIfFailed(output6->GetDesc1(&desc1));
    return desc1;
}

bool DXDeviceInfo::support_sdr_10()
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support;
    format_support.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &format_support, sizeof(format_support)));
    return (format_support.Support1 & D3D12_FORMAT_SUPPORT1_DISPLAY) != 0 ||
           (format_support.Support2 & D3D12_FORMAT_SUPPORT1_DISPLAY) != 0;
}

bool DXDeviceInfo::support_linear_sdr()
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support;
    format_support.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &format_support, sizeof(format_support)));
    return (format_support.Support1 & D3D12_FORMAT_SUPPORT1_DISPLAY) != 0 ||
           (format_support.Support2 & D3D12_FORMAT_SUPPORT1_DISPLAY) != 0;
}

auto DXDeviceInfo::CreateExternalDevice() noexcept -> luisa::optional<ExternalDevice>
{
    if (!device) { return {}; }
    return ExternalDevice{
        .device = device
    };
}

auto DXDeviceInfo::GetGPUAllocatorSettings() noexcept -> luisa::optional<GPUAllocatorSettings>
{
    return GPUAllocatorSettings{
        .preferred_block_size = 0,
        .sparse_buffer_block_size = 0,
        .sparse_image_block_size = 64ull * 1024ull * 1024ull,
    };
}
} // namespace rbc
#undef ThrowIfFailed