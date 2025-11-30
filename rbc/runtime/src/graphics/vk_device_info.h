#pragma once
#include <rbc_config.h>
#include <luisa/backends/ext/vk_config_ext.h>
#include <volk.h>

namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API VkDeviceInfo : public VulkanDeviceConfigExt {
    VkInstance instance{};
    VkPhysicalDevice physical_device{};
    VkDevice device{};
    VkAllocationCallbacks *alloc_callback{};
    VkQueue graphics_queue{};
    VkQueue compute_queue{};
    VkQueue copy_queue{};
    IDxcCompiler3 *dxc_compiler{};
    IDxcLibrary *dxc_library{};
    IDxcUtils *dxc_utils{};
    uint32_t graphics_queue_family_index{};
    uint32_t compute_queue_family_index{};
    uint32_t copy_queue_family_index{};
    luisa::vector<VKCustomCmd::ResourceUsage> resource_before_states;
    luisa::vector<VKCustomCmd::ResourceUsage> resource_after_states;
    VkDeviceInfo();
    ~VkDeviceInfo();
    void init_volk(PFN_vkGetInstanceProcAddr handler) noexcept override;
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
    luisa::span<VKCustomCmd::ResourceUsage const> before_states(uint64_t stream_handle) noexcept override {
        return resource_before_states;
    }
    luisa::span<VKCustomCmd::ResourceUsage const> after_states(uint64_t stream_handle) noexcept override {
        return resource_after_states;
    }
};
}// namespace rbc
