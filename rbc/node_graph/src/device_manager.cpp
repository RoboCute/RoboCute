#include <rbc_node/device_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/backends/ext/vk_cuda_interop.h>

namespace rbc {
namespace detail {
static void _check_device_desc(ComputeDeviceDesc const &desc) {
    switch (desc.type) {
        case ComputeDeviceType::HOST: {
            if (desc.device_index != 0) [[unlikely]] {
                LUISA_ERROR("Host device index must be 0");
            }
        } break;
        case ComputeDeviceType::RENDER_DEVICE: {
            if (desc.device_index != RenderDevice::instance().device_index()) [[unlikely]] {
                LUISA_ERROR("Render deivce index {} mismatch with singleton {}.", desc.device_index, RenderDevice::instance().device_index());
            }
        } break;
    }
}
}// namespace detail
DeviceManager::DeviceManager(Context &&lc_ctx)
    : _lc_ctx(std::move(lc_ctx)) {
    auto &render_device = RenderDevice::instance();
    if (render_device.backend_name() == "dx") {
        _interop_ext = render_device.lc_device().extension<DxCudaInterop>();
    } else if (render_device.backend_name() == "vk") {
        // cuda event for vulkan
        _interop_ext = render_device.lc_device().extension<VkCudaInterop>();
    }
}
DeviceManager::~DeviceManager() {}
void DeviceManager::add_device(ComputeDeviceDesc const &device_desc) {
    switch (device_desc.type) {
        case ComputeDeviceType::RENDER_DEVICE: {
            // TODO: init render-device in graphics_utils currently
            LUISA_ERROR("Render device can not be added manually.");
        } break;
        case ComputeDeviceType::HOST: break;
        case ComputeDeviceType::COMPUTE_DEVICE: {
            auto iter = _compute_devices.try_emplace(device_desc.device_index);
            if (!iter.second) {
                LUISA_WARNING("Trying to add same device, ignored.");
                break;
            }
            auto &v = iter.first.value();
            DeviceConfig device_config{
                .device_index = device_desc.device_index};
            v.device = _lc_ctx.create_device("cuda", &device_config);
            v.main_stream = v.device.create_stream();
            v.event = v.device.create_event();
        } break;
        default:
            LUISA_ERROR("Unsupported device {}", luisa::to_string(device_desc.type));
            break;
    }
}
void DeviceManager::make_synchronize(
    ComputeDeviceDesc const &src_device,
    ComputeDeviceDesc const &dst_device) {
    detail::_check_device_desc(src_device);
    detail::_check_device_desc(dst_device);
    // same device
    if (src_device == dst_device || src_device.type == ComputeDeviceType::HOST) return;

    // same device-type not sync
    if (src_device.type == dst_device.type) {
        switch (src_device.type) {
            case ComputeDeviceType::RENDER_DEVICE: {
                auto &stream = RenderDevice::instance().lc_main_stream();
                auto &cmdlist = RenderDevice::instance().lc_main_cmd_list();
                if (!cmdlist.empty()) {
                    stream << cmdlist.commit();
                }
                stream.synchronize();
            } break;
            case ComputeDeviceType::COMPUTE_DEVICE: {
                auto &v = _compute_device(src_device.device_index);
                if (!v.main_cmdlist.empty()) {
                    v.main_stream << v.main_cmdlist.commit() << v.event.signal();
                }
                v.event.synchronize();
            } break;
            default:
                LUISA_ERROR("Unsupported device.");
                break;
        }
    }
}
void DeviceManager::_create_interop_evt(uint compute_index, ComputeDevice &compute_device) {
    auto &render_device = RenderDevice::instance();
    auto &interop_evt = compute_device._interop_evt;
    if (!interop_evt.valid()) {
        if (render_device.backend_name() == "dx") {
            auto ext = static_cast<DxCudaInterop *>(_interop_ext);
            interop_evt = ext->create_timeline_event();
        } else if (render_device.backend_name() == "vk") {
            // cuda event for vulkan
            interop_evt = compute_device.device.create_timeline_event();
        } else {
            // TODO: other devices
            LUISA_ERROR("Deivce {} not supported.", render_device.backend_name());
        }
    }
}
void DeviceManager::_sync_render_to_compute(uint32_t compute_index) {
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();
    auto &stream = render_device.lc_main_stream();
    if (!cmdlist.empty()) {
        stream << cmdlist.commit();
    }
    if (render_device.device_index() != std::numeric_limits<uint64_t>::max() && render_device.device_index() != compute_index) {
        stream.synchronize();
        return;
    }
    auto &compute_device = _compute_device(compute_index);
    _create_interop_evt(compute_index, compute_device);
    auto &interop_evt = compute_device._interop_evt;
    interop_evt.visit([&]<typename T>(T const &t) {
        std::lock_guard lck{_render_device_mtx};
        auto fence = ++_render_device_fence_index;
        if constexpr (std::is_same_v<T, DxCudaTimelineEvent>) {
            stream << t.dx_signal(fence);
            compute_device.main_stream << t.cuda_wait(fence);
        } else {
            stream << static_cast<VkCudaInterop *>(_interop_ext)->vk_signal(t, fence);
            compute_device.main_stream << t.wait(fence);
        }
    });
}
ComputeDevice &DeviceManager::_compute_device(uint32_t index) {
    auto iter = _compute_devices.find(index);
    if (!iter) [[unlikely]] {
        LUISA_ERROR("Can not find compute device {}", index);
    }
    return iter.value();
}
void DeviceManager::_sync_compute_to_render(uint32_t compute_index) {
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();
    auto &stream = render_device.lc_main_stream();
    auto &compute_device = _compute_device(compute_index);
    if (render_device.device_index() != std::numeric_limits<uint64_t>::max() && render_device.device_index() != compute_index) {
        compute_device.event.synchronize();
        return;
    }
    _create_interop_evt(compute_index, compute_device);
    auto &interop_evt = compute_device._interop_evt;
    interop_evt.visit([&]<typename T>(T const &t) {
        std::lock_guard lck{_render_device_mtx};
        auto fence = ++_render_device_fence_index;
        if constexpr (std::is_same_v<T, DxCudaTimelineEvent>) {
            compute_device.main_stream << t.cuda_signal(fence);
            stream << t.dx_wait(fence);
        } else {
            compute_device.main_stream << t.signal(fence);
            stream << static_cast<VkCudaInterop *>(_interop_ext)->vk_wait(t, fence);
        }
    });
}
NodeBuffer DeviceManager::create_buffer(RC<BufferDescriptor> buffer_desc, ComputeDeviceDesc const &src_device_desc, ComputeDeviceDesc const &dst_device_desc) {
    detail::_check_device_desc(src_device_desc);
    detail::_check_device_desc(dst_device_desc);
    auto &desc = *buffer_desc.get();
    NodeBuffer node_buffer{std::move(buffer_desc), src_device_desc, dst_device_desc};
    // no consider interop
    if (src_device_desc == dst_device_desc || src_device_desc.device_index != src_device_desc.device_index || (!is_device_type(src_device_desc.type) || !is_device_type(dst_device_desc.type))) {
        switch (src_device_desc.type) {
            case ComputeDeviceType::RENDER_DEVICE: {
                node_buffer._buffer = RenderDevice::instance().lc_device().create_byte_buffer(desc.size_bytes());
            } break;
            case ComputeDeviceType::COMPUTE_DEVICE: {
                auto &compute_device = _compute_device(src_device_desc.device_index);
                node_buffer._buffer = compute_device.device.create_byte_buffer(desc.size_bytes());
            } break;
            case ComputeDeviceType::HOST: {
                node_buffer._buffer.reset_as<luisa::vector<std::byte>>();
                auto &vec = node_buffer._buffer.force_get<luisa::vector<std::byte>>();
                vec.push_back_uninitialized(desc.size_bytes());
            } break;
            default:
                LUISA_ERROR("Unsupported device {}", luisa::to_string(src_device_desc.type));
                break;
        }
        return node_buffer;
    }

    auto &render_device = RenderDevice::instance();
    if (render_device.backend_name() == "dx") {
        node_buffer._buffer = static_cast<DxCudaInterop *>(_interop_ext)->create_byte_buffer(desc.size_bytes());
    } else if (render_device.backend_name() == "vk") {
        node_buffer._buffer = static_cast<VkCudaInterop *>(_interop_ext)->create_byte_buffer(desc.size_bytes());
    } else {
        // TODO: other devices
        LUISA_ERROR("Deivce {} not supported.", render_device.backend_name());
    }
    node_buffer._is_interop = true;
    return node_buffer;
}
}// namespace rbc