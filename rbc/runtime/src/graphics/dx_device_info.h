#pragma once
#include <rbc_config.h>
#include <luisa/backends/ext/dx_config_ext.h>
#include <rbc_graphics/make_device_config.h>
#include <dxgi1_6.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct DXDeviceInfo : public DirectXDeviceConfigExt {
public:
    ID3D12Device *device{};
    IDXGIAdapter1 *adapter{};
    IDXGIFactory2 *factory{};
    DirectXFuncTable const *funcTable{};
    luisa::BinaryIO const *shaderIo{};
    IDxcCompiler3 *dxcCompiler{};
    IDxcLibrary *dxcLibrary{};
    IDxcUtils *dxcUtils{};
    ID3D12DescriptorHeap *shaderDescriptor{};
    ID3D12DescriptorHeap *samplerDescriptor{};
    bool gpu_dump = false;
    luisa::vector<DXCustomCmd::EnhancedResourceUsage> resource_before_states;
    luisa::vector<DXCustomCmd::EnhancedResourceUsage> resource_after_states;
    DXDeviceInfo(
        ID3D12Device *device)
        : device(device) {
    }
    RBC_RUNTIME_API luisa::optional<ExternalDevice> CreateExternalDevice() noexcept override;
    RBC_RUNTIME_API luisa::optional<GPUAllocatorSettings> GetGPUAllocatorSettings() noexcept override;
    RBC_RUNTIME_API void ReadbackDX12Device(
        ID3D12Device *device,
        IDXGIAdapter1 *adapter,
        IDXGIFactory2 *factory,
        DirectXFuncTable const *funcTable,
        luisa::BinaryIO const *shaderIo,
        IDxcCompiler3 *dxcCompiler,
        IDxcLibrary *dxcLibrary,
        IDxcUtils *dxcUtils,
        ID3D12DescriptorHeap *shaderDescriptor,
        ID3D12DescriptorHeap *samplerDescriptor) noexcept override;

    RBC_RUNTIME_API DXGI_OUTPUT_DESC1 GetOutput(HWND window_handle);
    RBC_RUNTIME_API bool support_sdr_10();
    RBC_RUNTIME_API bool support_linear_sdr();
    bool UseDRED() const noexcept override {
        return gpu_dump;
    }
    luisa::span<DXCustomCmd::EnhancedResourceUsage const> after_states(uint64_t stream_handle) noexcept override  {
        return resource_after_states;
    }
    luisa::span<DXCustomCmd::EnhancedResourceUsage const> before_states(uint64_t stream_handle) noexcept override {
        return resource_before_states;
    }
    ~DXDeviceInfo();
};
}// namespace rbc