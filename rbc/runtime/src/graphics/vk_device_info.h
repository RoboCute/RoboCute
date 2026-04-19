#pragma once
#include <rbc_config.h>
#include <luisa/backends/ext/vk_config_ext.h>
#include <rbc_graphics/make_device_config.h>
#include <volk.h>

namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API VkDeviceInfo : public VulkanDeviceConfigExt {

    VkDeviceInfo() = default;
    ~VkDeviceInfo() = default;
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
        return _resource_before_states;
    }
    luisa::span<VKCustomCmd::ResourceUsage const> after_states(uint64_t stream_handle) noexcept override {
        return _resource_after_states;
    }

private:
    VkInstance _instance{};
    VkPhysicalDevice _physical_device{};
    VkDevice _device{};
    VkAllocationCallbacks *_alloc_callback{};
    VkQueue _graphics_queue{};
    VkQueue _compute_queue{};
    VkQueue _copy_queue{};
    IDxcCompiler3 *_dxc_compiler{};
    IDxcLibrary *_dxc_library{};
    IDxcUtils *_dxc_utils{};
    uint32_t _graphics_queue_family_index{};
    uint32_t _compute_queue_family_index{};
    uint32_t _copy_queue_family_index{};
    luisa::vector<VKCustomCmd::ResourceUsage> _resource_before_states;
    luisa::vector<VKCustomCmd::ResourceUsage> _resource_after_states;

    friend RBC_RUNTIME_API luisa::unique_ptr<luisa::compute::DeviceConfigExt> make_vk_device_config(
        void *device,
        bool gpu_dump);
    friend RBC_RUNTIME_API void clear_vk_states(
        luisa::compute::DeviceConfigExt *device_config_ext);
    friend RBC_RUNTIME_API void add_vk_before_state(
        luisa::compute::DeviceConfigExt *device_config_ext,
        luisa::variant<
            luisa::compute::Argument::Buffer,
            luisa::compute::Argument::Texture,
            luisa::compute::Argument::BindlessArray> const &resource,
        VkResourceUsageType resource_type);
    friend RBC_RUNTIME_API void add_vk_after_state(
        luisa::compute::DeviceConfigExt *device_config_ext,
        luisa::variant<
            luisa::compute::Argument::Buffer,
            luisa::compute::Argument::Texture,
            luisa::compute::Argument::BindlessArray> const &resource,
        VkResourceUsageType resource_type);
    friend RBC_RUNTIME_API void get_vk_device(
        luisa::compute::DeviceConfigExt *device_config_ext,
        void *&device,
        void *&physical_device,
        void *&vk_instance,
        uint32_t &gfx_queue_family_index);
};
}// namespace rbc
