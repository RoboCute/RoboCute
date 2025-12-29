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
#include <luisa/backends/ext/dx_cuda_interop.h>

namespace rbc {
using namespace luisa::compute;

struct ComputeDevice {
    Device device;
    Stream main_stream;
    Event event;
    CommandList main_cmdlist;
    rbc::shared_atomic_mutex _mtx;
    vstd::variant<
        DxCudaTimelineEvent,
        TimelineEvent>
        _interop_evt;
};
struct RBC_NODE_API DeviceManager {
private:
    Context _lc_ctx;
    vstd::HashMap<uint32_t, ComputeDevice> _compute_devices;
    uint64_t _render_device_fence_index{};
    luisa::spin_mutex _render_device_mtx;
    void *_interop_ext{};
    void _sync_render_to_compute(uint32_t compute_index);
    void _sync_compute_to_render(uint32_t compute_index);
    ComputeDevice &_compute_device(uint32_t index);
    void _create_interop_evt(uint compute_index, ComputeDevice &compute_device);

public:
    DeviceManager(Context &&lc_ctx);
    ~DeviceManager();
    void add_device(ComputeDeviceDesc const &device_desc);
    void make_synchronize(
        ComputeDeviceDesc const &src_device,
        ComputeDeviceDesc const &dst_device);
    NodeBuffer create_buffer(RC<BufferDescriptor> buffer_desc, ComputeDeviceDesc const &src_device_desc, ComputeDeviceDesc const &dst_device_desc);
};
}// namespace rbc