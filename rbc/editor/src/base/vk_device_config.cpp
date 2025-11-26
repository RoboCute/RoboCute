
#include "RBCEditor/base/device_config.h"
#include "RBCEditor/base/vk_device_config.h"
#include <volk.h>
#include <luisa/backends/ext/vk_custom_cmd.h>
#include <luisa/runtime/image.h>
VkDeviceConfig::VkDeviceConfig() {}
VkDeviceConfig::~VkDeviceConfig() {}
void VkDeviceConfig::readback_vulkan_device(
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
}
luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_vk_device_config(
    void *device,
    void *vk_instance,
    void *vk_physical_device) {
    auto device_config = luisa::make_unique<VkDeviceConfig>();
    device_config->instance = reinterpret_cast<VkInstance>(vk_instance);
    device_config->physical_device = reinterpret_cast<VkPhysicalDevice>(vk_physical_device);
    device_config->device = reinterpret_cast<VkDevice>(device);
    return device_config;
}
auto VkDeviceConfig::create_external_device() noexcept -> ExternalDevice {
    return ExternalDevice{
        .instance = instance,
        .physical_device = physical_device,
        .device = device};
}
void get_vk_device(
    luisa::compute::DeviceConfigExt *device_config_ext,
    void *&device,
    void *&physical_device,
    void *&vk_instance,
    uint32_t &gfx_queue_family_index) {
    auto ptr = static_cast<VkDeviceConfig *>(device_config_ext);
    device = ptr->device;
    physical_device = ptr->physical_device;
    vk_instance = ptr->instance;
    gfx_queue_family_index = ptr->graphics_queue_family_index;
}

void set_vk_before_state(
    luisa::compute::DeviceConfigExt *device_config_ext,
    luisa::variant<
        luisa::compute::Argument::Buffer,
        luisa::compute::Argument::Texture,
        luisa::compute::Argument::BindlessArray> const &resource,
    VkResourceUsageType resource_type) {
    auto ptr = static_cast<VkDeviceConfig *>(device_config_ext);
    ptr->resource_before_states.clear();
    ptr->resource_before_states.emplace_back(
        resource,
        (VKCustomCmd::ResourceUsageType)resource_type);
}
void *VkDeviceConfig::device_feature_settings() noexcept {
    multiview_features = VkPhysicalDeviceMultiviewFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
        .multiview = VK_TRUE};
    return &multiview_features;
}