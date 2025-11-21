#include <rbc_graphics/render_device.h>
#include <rbc_graphics/make_device_config.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/image.h>

namespace rbc
{
namespace render_device_detail
{
static RenderDevice* _inst = nullptr;
}
RenderDevice& RenderDevice::instance()
{
    return *render_device_detail::_inst;
}
void RenderDevice::set_instance(RenderDevice* device)
{
    LUISA_ASSERT(device != nullptr, "Device must not be nullptr");
    render_device_detail::_inst = device;
}
void RenderDevice::init(
    const luisa::filesystem::path& program_path,
    luisa::string_view backend,
    bool headless,
    bool require_async_stream, bool require_io_service, bool gpu_dump, void* external_device
)
{
    set_instance(this);
    _backend = backend;
    _headless = headless;
    // create context
    _context.create(luisa::to_string(program_path));

    // create device
    luisa::unique_ptr<luisa::compute::DeviceConfigExt> ext;
    if (backend == "dx")
        ext = rbc::make_dx_device_config(external_device, gpu_dump);
    else if (backend == "vk")
        ext = rbc::make_vk_device_config(external_device, gpu_dump);

    DeviceConfig config{
        .extension = std::move(ext),
        .inqueue_buffer_limit = false
    };
    _device_ext = config.extension.get();
    _device = _context.value().create_device(
        backend, &config,
    // Enable validation-layer for debug mode
#ifdef NDEBUG
        false
#else
        true
#endif
    );
    if (!_headless)
    {
        // create main stream
        _managed_device = Device{ luisa::make_unique<ManagedDevice>(Context{ lc_ctx() }, _device.impl()) };
        _main_stream = _device.create_stream(StreamTag::GRAPHICS);
        _main_stream.set_name("main_stream");
        _main_stream.set_log_callback([](auto sv) {
            LUISA_INFO("GPU Direct Queue: {}", sv);
        });
        if (require_async_stream)
        {
            _async_stream = _device.create_stream(StreamTag::COMPUTE);
            _async_stream.set_name("load_stream");
            _async_stream.set_log_callback([](auto sv) {
                LUISA_INFO("GPU Compute Queue: {}", sv);
            });
            _async_copy_stream = _device.create_stream(StreamTag::COPY);
            _async_copy_stream.set_name("copy_stream");
            _async_copy_stream.set_log_callback([](auto sv) {
                LUISA_INFO("GPU Copy Queue: {}", sv);
            });
            _async_timeline = _device.create_timeline_event();
        }
        if (require_io_service)
        {
            IOService::init(backend == "dx" ? IOService::QueueType::DX12 : IOService::QueueType::Fallback, _context.value().runtime_directory(), false);

            _io_service = IOService::create_service(_device, DStorageSrcType::File);
            _mem_io_service = IOService::create_service(_device, DStorageSrcType::Memory);
        }
    }
}
void RenderDevice::shutdown()
{
    if (!_headless)
    {
        if (_mem_io_service)
            IOService::dispose_service(_mem_io_service);
        if (_io_service)
            IOService::dispose_service(_io_service);
        IOService::dispose();
        if (_async_stream)
            _async_stream.synchronize();
        if (_async_copy_stream)
            _async_copy_stream.synchronize();
    }
    _managed_device = {};
    _device = {};
    _context.reset();
}

} // namespace rbc
