#pragma once
#include <rbc_config.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/event.h>
#include <luisa/core/fiber.h>
#include <luisa/vstl/common.h>
#include <rbc_node/node_buffer.h>

namespace rbc {
using namespace luisa::compute;

struct ComputeDevice {
    Device device;
    Stream main_stream;
    Event event;
    CommandList main_cmdlist;
    rbc::shared_atomic_mutex _mtx;
};
struct RBC_NODE_API DeviceManager {
private:
    Context _lc_ctx;
    vstd::HashMap<uint32_t, ComputeDevice> _compute_devices;
    void _sync_render_to_compute(uint32_t compute_index);
    void _sync_compute_to_render(uint32_t compute_index);

public:
    DeviceManager(Context &&lc_ctx);
    ~DeviceManager();
    void add_device(ComputeDeviceDesc const &device_desc);
    void make_synchronize(
        ComputeDeviceDesc const &src_device,
        ComputeDeviceDesc const &dst_device);
};
}// namespace rbc