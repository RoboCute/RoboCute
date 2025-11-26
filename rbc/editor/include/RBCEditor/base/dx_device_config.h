#pragma once
#include <luisa/backends/ext/dx_config_ext.h>
#include <dxgi1_6.h>
using namespace luisa;
using namespace luisa::compute;
struct DXDeviceConfig : public DirectXDeviceConfigExt {
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
    luisa::vector<DXCustomCmd::EnhancedResourceUsage> resource_before_states;
    bool gpu_dump = false;
    DXDeviceConfig(
        ID3D12Device *device)
        : device(device) {
    }
    luisa::optional<ExternalDevice> CreateExternalDevice() noexcept override;
    luisa::optional<GPUAllocatorSettings> GetGPUAllocatorSettings() noexcept override;
    bool LoadDXC() const noexcept override {
        return true;// TODO: we may don't want to load DXC in random target platform
    }
    void ReadbackDX12Device(
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

    DXGI_OUTPUT_DESC1 GetOutput(HWND window_handle);
    bool support_sdr_10();
    bool support_linear_sdr();
    bool UseDRED() const noexcept override {
        return gpu_dump;
    }
    luisa::span<DXCustomCmd::EnhancedResourceUsage const> before_states(uint64_t stream_handle) noexcept override {
        return resource_before_states;
    }
};