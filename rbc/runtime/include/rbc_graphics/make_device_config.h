#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/memory.h>
#include <luisa/runtime/rhi/device_interface.h>
namespace rbc
{
// All device config inherit base
RBC_RUNTIME_API luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_dx_device_config(
    void* device,
    bool gpu_dump);
RBC_RUNTIME_API luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_vk_device_config(
    void* device,
    bool gpu_dump);
}