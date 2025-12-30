#include <rbc_node/device_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/dsl/sugar.h>
#include <luisa/runtime/shader.h>

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
    const uint size = 1024;
    RC<BufferDescriptor> buffer_desc{new BufferDescriptor{
        Type::of<float>(),
        size}};
    ComputeDeviceDesc compute_device_desc{
        .type = ComputeDeviceType::COMPUTE_DEVICE};
    ComputeDeviceDesc render_device_desc{
        .type = ComputeDeviceType::RENDER_DEVICE};
    device_mng.add_device(compute_device_desc);
    auto interop_buffer = device_mng.create_buffer(buffer_desc, compute_device_desc, render_device_desc);
    Kernel1D add_kernel{[](BufferVar<float> buffer, Float value, Bool add) {
        Float v;
        $if (add) {
            v = buffer.read(dispatch_id().x);
        };
        buffer.write(dispatch_id().x, dispatch_id().x.cast<float>() + value + v);
    }};
    auto render_shader = render_device.lc_device().compile(add_kernel);
    auto& compute_device = device_mng.get_compute_device(compute_device_desc.device_index);
    auto compute_shader = compute_device.device.compile(add_kernel);
    // Compute(CUDA) logic
    {
        compute_device.main_cmdlist
            << compute_shader(
                   device_mng.get_buffer(interop_buffer, compute_device_desc).as<float>(), 114, 0)
                   .dispatch(size);
    }
    // sync
    device_mng.make_synchronize(compute_device_desc, render_device_desc);
    // Render (DX12/VK) logic
    luisa::vector<float> data(size);
    {
        auto buffer = device_mng.get_buffer(interop_buffer, render_device_desc);
        render_device.lc_main_stream()
            << render_shader(
                   buffer.as<float>(), 514, 1)
                   .dispatch(size)
            << buffer.copy_to(data.data()) << synchronize();
    }
    // result
    double average{};
    for(auto& i : data) {
        average += (double)i / (double)size;
    }
    LUISA_INFO("Result {}", average);
    return 0;
}