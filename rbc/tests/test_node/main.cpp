#include <rbc_node/device_manager.h>
#include <rbc_graphics/render_device.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        LUISA_ERROR("Must have backend.");
    }
    using namespace rbc;
    RenderDevice render_device;
    render_device.init(argv[0], argv[1]);
    RenderDevice::set_rendering_thread(true);
    auto main_stream = render_device.lc_device().create_stream(StreamTag::GRAPHICS);
    render_device.set_main_stream(&main_stream);

    DeviceManager device_mng{Context{render_device.lc_ctx()}};
    RC<BufferDescriptor> buffer_desc{new BufferDescriptor{
        Type::of<int>(),
        1024}};
    ComputeDeviceDesc compute_device_desc{
        .type = ComputeDeviceType::COMPUTE_DEVICE
    };
    ComputeDeviceDesc render_device_desc{
        .type = ComputeDeviceType::RENDER_DEVICE
    };
    device_mng.add_device(compute_device_desc);
    auto render_to_compute = device_mng.create_buffer(buffer_desc, compute_device_desc, render_device_desc);
    auto compute_to_render = device_mng.create_buffer(buffer_desc, render_device_desc, compute_device_desc);

    // test sync
    device_mng.make_synchronize(compute_device_desc, render_device_desc);
}