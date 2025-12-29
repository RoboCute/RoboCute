#include <rbc_node/device_manager.h>
#include <rbc_graphics/render_device.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        LUISA_ERROR("Must have backend.");
    }
    using namespace rbc;
    RenderDevice render_device;
    render_device.init(argv[0], argv[1]);
    DeviceManager device_mng{Context{render_device.lc_ctx()}};
    RC<BufferDescriptor> buffer_desc{new BufferDescriptor{
        Type::of<int>(),
        1024}};
    ComputeDeviceDesc src_device_desc;
    ComputeDeviceDesc dst_device_desc;
    auto render_to_compute = device_mng.create_buffer(std::move(buffer_desc), src_device_desc, dst_device_desc);
}