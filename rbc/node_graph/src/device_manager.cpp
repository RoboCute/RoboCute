#include <rbc_node/device_manager.h>
#include <rbc_graphics/render_device.h>

namespace rbc {
DeviceManager::DeviceManager(Context &&lc_ctx)
    : _lc_ctx(std::move(lc_ctx)) {}
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
                auto iter = _compute_devices.find(src_device.device_index);
                if (!iter) [[unlikely]] {
                    LUISA_ERROR("Can not find compute device {}", src_device.device_index);
                }
                auto &v = iter.value();
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
void DeviceManager::_sync_render_to_compute(uint32_t compute_index) {}
void DeviceManager::_sync_compute_to_render(uint32_t compute_index) {
}
}// namespace rbc