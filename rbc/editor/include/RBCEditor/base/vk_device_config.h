#pragma once

#include <volk.h>
#include <luisa/backends/ext/vk_config_ext.h>

using namespace luisa;
using namespace luisa::compute;
struct VkDeviceConfig : public VulkanDeviceConfigExt {
    VkInstance instance{};
    VkPhysicalDevice physical_device{};
    VkDevice device{};
    VkAllocationCallbacks *alloc_callback{};
    VkQueue graphics_queue{};
    VkQueue compute_queue{};
    VkQueue copy_queue{};
    uint32_t graphics_queue_family_index{};
    uint32_t compute_queue_family_index{};
    uint32_t copy_queue_family_index{};
    IDxcCompiler3 *dxc_compiler{};
    IDxcLibrary *dxc_library{};
    IDxcUtils *dxc_utils{};
    luisa::vector<VKCustomCmd::ResourceUsage> resource_before_states;
    VkPhysicalDeviceMultiviewFeatures multiview_features;
    VkDeviceConfig();
    ~VkDeviceConfig();
    bool load_dxc() const noexcept override {
        return true;// TODO: we may don't want to load DXC in random target platform
    }
    luisa::span<VKCustomCmd::ResourceUsage const> before_states(uint64_t stream_handle) noexcept override {
        return resource_before_states;
    }
    luisa::vector<luisa::string> extra_device_exts() noexcept {
        return luisa::vector<luisa::string>{"VK_KHR_multiview", "VK_KHR_maintenance2"};
    }
    ExternalDevice create_external_device() noexcept;
    void readback_vulkan_device(
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
        IDxcUtils *dxc_utils) noexcept override;
    void *device_feature_settings() noexcept override;
};