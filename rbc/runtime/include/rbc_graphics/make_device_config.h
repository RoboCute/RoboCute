#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/memory.h>
#include <luisa/runtime/rhi/device_interface.h>
namespace rbc {
// All device config inherit base
RBC_RUNTIME_API luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_dx_device_config(
    void *device,
    bool gpu_dump);
RBC_RUNTIME_API luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_vk_device_config(
    void *device,
    bool gpu_dump);

enum class D3D12EnhancedResourceUsageType : uint32_t {
    ComputeRead,
    ComputeAccelRead,
    ComputeUAV,
    CopySource,
    CopyDest,
    BuildAccel,
    CopyAccelSrc,
    CopyAccelDst,
    DepthRead,
    DepthWrite,
    IndirectArgs,
    VertexRead,
    IndexRead,
    RenderTarget,
    AccelInstanceBuffer,
    RasterRead,
    RasterAccelRead,
    RasterUAV,
    VideoEncodeRead,
    VideoEncodeWrite,
    VideoProcessRead,
    VideoProcessWrite,
    VideoDecodeRead,
    VideoDecodeWrite,
};
enum class VkResourceUsageType {
    ComputeRead,
    ComputeAccelRead,
    ComputeUAV,
    CopySource,
    CopyDest,
    BuildAccel,
    BuildAccelScratch,
    CopyAccelSrc,
    CopyAccelDst,
    DepthRead,
    DepthWrite,
    IndirectArgs,
    VertexRead,
    IndexRead,
    RenderTarget,
    AccelInstanceBuffer,
    RasterRead,
    RasterAccelRead,
    RasterUAV,
};

RBC_RUNTIME_API void clear_dx_states(
    luisa::compute::DeviceConfigExt *device_config_ext);

RBC_RUNTIME_API void clear_vk_states(
    luisa::compute::DeviceConfigExt *device_config_ext);

RBC_RUNTIME_API void add_dx_before_state(
    luisa::compute::DeviceConfigExt *device_config_ext,
    luisa::variant<
        luisa::compute::Argument::Buffer,
        luisa::compute::Argument::Texture,
        luisa::compute::Argument::BindlessArray> const &resource,
    D3D12EnhancedResourceUsageType resource_type);

RBC_RUNTIME_API void add_vk_before_state(
    luisa::compute::DeviceConfigExt *device_config_ext,
    luisa::variant<
        luisa::compute::Argument::Buffer,
        luisa::compute::Argument::Texture,
        luisa::compute::Argument::BindlessArray> const &resource,
    VkResourceUsageType resource_type);

RBC_RUNTIME_API void add_dx_after_state(
    luisa::compute::DeviceConfigExt *device_config_ext,
    luisa::variant<
        luisa::compute::Argument::Buffer,
        luisa::compute::Argument::Texture,
        luisa::compute::Argument::BindlessArray> const &resource,
    D3D12EnhancedResourceUsageType resource_type);

RBC_RUNTIME_API void add_vk_after_state(
    luisa::compute::DeviceConfigExt *device_config_ext,
    luisa::variant<
        luisa::compute::Argument::Buffer,
        luisa::compute::Argument::Texture,
        luisa::compute::Argument::BindlessArray> const &resource,
    VkResourceUsageType resource_type);

RBC_RUNTIME_API void get_dx_device(
    luisa::compute::DeviceConfigExt *device_config_ext,
    void *&device,
    luisa::uint2 &adaptor_luid);

RBC_RUNTIME_API void get_vk_device(
    luisa::compute::DeviceConfigExt *device_config_ext,
    void *&device,
    void *&physical_device,
    void *&vk_instance,
    uint32_t &gfx_queue_family_index);
}// namespace rbc
