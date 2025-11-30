#include <volk.h>
#include "vk_device_info.h"
#include <rbc_graphics/make_device_config.h>
namespace rbc {
VkDeviceInfo::VkDeviceInfo() {}
VkDeviceInfo::~VkDeviceInfo() {}
void VkDeviceInfo::init_volk(PFN_vkGetInstanceProcAddr handler) noexcept {
    volkInitializeCustom(handler);
}
void VkDeviceInfo::readback_vulkan_device(
    VkInstance instance,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkAllocationCallbacks *alloc_callback,
    VkPipelineCacheHeaderVersionOne const &pso_meta,
    VkQueue graphics_queue,
    VkQueue compute_queue,
    VkQueue copy_queue,
    uint32_t graphics_queue_family_index,
    uint32_t compute_queue_family_index,
    uint32_t copy_queue_family_index,
    IDxcCompiler3 *dxc_compiler,
    IDxcLibrary *dxc_library,
    IDxcUtils *dxc_utils) noexcept {
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
    this->graphics_queue_family_index = graphics_queue_family_index;
    this->compute_queue_family_index = compute_queue_family_index;
    this->copy_queue_family_index = copy_queue_family_index;
    volkLoadInstance(instance);
}
RBC_RUNTIME_API luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_vk_device_config(
    void *device,
    bool gpu_dump) {
    // TODO : device and gpu_dump
    return luisa::make_unique<rbc::VkDeviceInfo>();
}
RBC_RUNTIME_API void clear_vk_states(
    luisa::compute::DeviceConfigExt *device_config_ext) {
    auto ptr = static_cast<VkDeviceInfo *>(device_config_ext);
    ptr->resource_before_states.clear();
    ptr->resource_after_states.clear();
}
RBC_RUNTIME_API void add_vk_before_state(
    luisa::compute::DeviceConfigExt *device_config_ext,
    luisa::variant<
        luisa::compute::Argument::Buffer,
        luisa::compute::Argument::Texture,
        luisa::compute::Argument::BindlessArray> const &resource,
    VkResourceUsageType resource_type) {
    auto ptr = static_cast<VkDeviceInfo *>(device_config_ext);
    ptr->resource_before_states.emplace_back(
        resource,
        (VKCustomCmd::ResourceUsageType)resource_type);
}

RBC_RUNTIME_API void add_vk_after_state(
    luisa::compute::DeviceConfigExt *device_config_ext,
    luisa::variant<
        luisa::compute::Argument::Buffer,
        luisa::compute::Argument::Texture,
        luisa::compute::Argument::BindlessArray> const &resource,
    VkResourceUsageType resource_type) {
    auto ptr = static_cast<VkDeviceInfo *>(device_config_ext);
    ptr->resource_after_states.emplace_back(
        resource,
        (VKCustomCmd::ResourceUsageType)resource_type);
}

RBC_RUNTIME_API void get_vk_device(
    luisa::compute::DeviceConfigExt *device_config_ext,
    void *&device,
    void *&physical_device,
    void *&vk_instance,
    uint32_t &gfx_queue_family_index) {
    auto ptr = static_cast<VkDeviceInfo *>(device_config_ext);
    device = ptr->device;
    physical_device = ptr->physical_device;
    vk_instance = ptr->instance;
    gfx_queue_family_index = ptr->graphics_queue_family_index;
}
}// namespace rbc