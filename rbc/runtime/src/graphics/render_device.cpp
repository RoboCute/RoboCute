#include <rbc_graphics/render_device.h>
#include <rbc_graphics/make_device_config.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/image.h>

namespace rbc {
static thread_local bool _is_rendering_thread{false};
bool RenderDevice::is_rendering_thread() {
    return _is_rendering_thread;
}
void RenderDevice::set_rendering_thread(bool is_rendering_thread) {
    _is_rendering_thread = is_rendering_thread;
}
namespace render_device_detail {
static RenderDevice *_inst = nullptr;
}// namespace render_device_detail
RenderDevice &RenderDevice::instance() {
    return *render_device_detail::_inst;
}
RenderDevice *RenderDevice::instance_ptr() {
    return render_device_detail::_inst;
}
void RenderDevice::set_instance(RenderDevice *device) {
    LUISA_ASSERT(device != nullptr, "Device must not be nullptr");
    render_device_detail::_inst = device;
}
void RenderDevice::set_main_stream(Stream *main_stream) {
    _render_loop_mtx.lock();
    _main_stream_ptr = main_stream;
    _render_loop_mtx.unlock();
}
void RenderDevice::init(
    const luisa::filesystem::path &program_path,
    luisa::string_view backend,
    bool headless,
    bool require_async_stream, bool require_io_service, bool gpu_dump, void *external_device) {
    set_rendering_thread(true);
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
        .extension = std::move(ext)};
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
    if (!_headless) {
        // create main stream
        _managed_device = Device{luisa::make_unique<ManagedDevice>(Context{lc_ctx()}, _device.impl())};
        if (require_async_stream) {
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
        if (require_io_service) {
            auto queue_type = backend == "dx" ? IOService::QueueType::DX12 : IOService::QueueType::Fallback;
            IOService::init(queue_type, _context.value().runtime_directory(), false);

            _io_service = IOService::create_service(_device, DStorageSrcType::File);
            _mem_io_service = IOService::create_service(_device, DStorageSrcType::Memory);
            if (queue_type != IOService::QueueType::Fallback) {
                _fallback_mem_io_service = IOService::create_service(_device, DStorageSrcType::Memory, IOService::QueueType::Fallback);
            } else {
                _fallback_mem_io_service = _mem_io_service;
            }
        }
    }
}
RenderDevice::~RenderDevice() {
    set_rendering_thread(false);
    if (!_headless) {
        if (_fallback_mem_io_service && _fallback_mem_io_service != _mem_io_service)
            IOService::dispose_service(_fallback_mem_io_service);
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
    render_device_detail::_inst = nullptr;
}

}// namespace rbc
