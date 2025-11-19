#include <volk.h>
#include "vk_device_info.h"
namespace rbc
{
VkDeviceInfo::VkDeviceInfo() {}
VkDeviceInfo::~VkDeviceInfo() {}
void VkDeviceInfo::init_volk(PFN_vkGetInstanceProcAddr handler) noexcept
{
    volkInitializeCustom(handler);
}
void VkDeviceInfo::readback_vulkan_device(
    VkInstance instance,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkAllocationCallbacks* alloc_callback,
    VkPipelineCacheHeaderVersionOne const& pso_meta,
    VkQueue graphics_queue,
    VkQueue compute_queue,
    VkQueue copy_queue,
    uint32_t graphics_queue_family_index,
    uint32_t compute_queue_family_index,
    uint32_t copy_queue_family_index,
    IDxcCompiler3* dxc_compiler,
    IDxcLibrary* dxc_library,
    IDxcUtils* dxc_utils
) noexcept
{
    this->instance = instance;
    this->physical_device = physical_device;
    this->device = device;
    this->alloc_callback = alloc_callback;
    this->graphics_queue = graphics_queue;
    this->compute_queue = compute_queue;
    this->copy_queue = copy_queue;
    this->dxc_compiler = dxc_compiler;
    this->dxc_library = dxc_library;
    this->dxc_utils = dxc_utils;
    volkLoadInstance(instance);
}
RBC_RUNTIME_API luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_vk_device_config(
    void* device,
    bool gpu_dump
)
{
    // TODO : device and gpu_dump
    return luisa::make_unique<rbc::VkDeviceInfo>();
}
} // namespace rbc