#pragma once
#include <rbc_io/io_service.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/runtime/context.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_graphics/managed_device.h>

namespace rbc {
struct ManagedDevice;
using namespace luisa::compute;

struct RBC_RUNTIME_API RenderDevice {
    // ctor & dtor
    RenderDevice() = default;
    ~RenderDevice();

    // delete copy & move
    RenderDevice(const RenderDevice &) = delete;
    RenderDevice &operator=(const RenderDevice &) = delete;
    RenderDevice(RenderDevice &&) = delete;
    RenderDevice &operator=(RenderDevice &&) = delete;
    vstd::string _pipeline_name_;

    // init & shutdown
    void init(const luisa::filesystem::path &program_path, luisa::string_view backend = "dx", bool headless = false, bool require_async_stream = true, bool require_io_service = true, bool gpu_dump = false, void *external_device = nullptr, size_t device_index = std::numeric_limits<size_t>::max());
    template<typename T>
    Buffer<T> create_transient_buffer(vstd::string name, size_t element_count) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ + std::move(name));
        return _managed_device.create_buffer<T>(element_count);
    }
    template<typename T>
    Buffer<T> get_transient_buffer(vstd::string name, size_t element_count) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name), true);
        return _managed_device.create_buffer<T>(element_count);
    }
    template<typename T>
    Image<T> create_transient_image(vstd::string name, PixelStorage pixel, uint width, uint height, uint mip_levels = 1u, bool simultaneous_access = false, bool allow_raster_target = false) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name));
        return _managed_device.create_image<T>(pixel, width, height, mip_levels, simultaneous_access, allow_raster_target);
    }
    template<typename T>
    Image<T> get_transient_image(vstd::string name, PixelStorage pixel, uint width, uint height, uint mip_levels = 1u, bool simultaneous_access = false, bool allow_raster_target = false) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name), true);
        return _managed_device.create_image<T>(pixel, width, height, mip_levels, simultaneous_access, allow_raster_target);
    }
    template<typename T>
    Volume<T> create_transient_volume(vstd::string name, PixelStorage pixel, uint width, uint height, uint depth, uint mip_levels = 1u, bool simultaneous_access = false) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name));
        return _managed_device.create_volume<T>(pixel, width, height, depth, mip_levels, simultaneous_access, false);
    }
    template<typename T>
    Volume<T> get_transient_volume(vstd::string name, PixelStorage pixel, uint width, uint height, uint depth, uint mip_levels = 1u, bool simultaneous_access = false) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name), true);
        return _managed_device.create_volume<T>(pixel, width, height, depth, mip_levels, simultaneous_access, false);
    }
    template<typename T>
    Image<T> create_transient_image(vstd::string name, PixelStorage pixel, uint2 size, uint mip_levels = 1u, bool simultaneous_access = false, bool allow_raster_target = false) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name));
        return _managed_device.create_image<T>(pixel, size, mip_levels, simultaneous_access, allow_raster_target);
    }
    template<typename T>
    Image<T> get_transient_image(vstd::string name, PixelStorage pixel, uint2 size, uint mip_levels = 1u, bool simultaneous_access = false, bool allow_raster_target = false) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name), true);
        return _managed_device.create_image<T>(pixel, size, mip_levels, simultaneous_access, allow_raster_target);
    }
    template<typename T>
    Volume<T> create_transient_volume(vstd::string name, PixelStorage pixel, uint3 size, uint mip_levels = 1u, bool simultaneous_access = false) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name));
        return _managed_device.create_volume<T>(pixel, size, mip_levels, simultaneous_access, false);
    }
    template<typename T>
    Volume<T> get_transient_volume(vstd::string name, PixelStorage pixel, uint3 size, uint mip_levels = 1u, bool simultaneous_access = false) {
        auto d = static_cast<ManagedDevice *>(_managed_device.impl());
        d->set_next_res_name(_pipeline_name_ +std::move(name), true);
        return _managed_device.create_volume<T>(pixel, size, mip_levels, simultaneous_access, false);
    }

    // getter
    inline luisa::string_view backend_name() const { return _backend; }
    inline luisa::compute::Context &lc_ctx() { return _context.value(); }
    inline luisa::compute::Device &_lc_managed_device() { return _managed_device; }
    inline luisa::compute::Device &lc_device() { return _device; }
    inline const luisa::compute::Device &lc_device() const { return _device; }
    inline luisa::compute::DeviceConfigExt *lc_device_ext() { return _device_ext; }
    inline luisa::compute::Stream &lc_main_stream() { return *_main_stream_ptr; }
    const inline luisa::compute::Stream &lc_main_stream() const { return *_main_stream_ptr; }
    inline auto &lc_async_stream() { return _async_stream; }
    inline auto &lc_async_copy_stream() { return _async_copy_stream; }
    inline auto &lc_async_timeline() { return _async_timeline; }
    inline auto io_service() { return _io_service; }
    inline auto mem_io_service() { return _mem_io_service; }
    inline auto device_index() { return _device_index; }
    inline auto fallback_mem_io_service() { return _fallback_mem_io_service; }
    inline auto &lc_main_cmd_list() { return _main_cmd_list; }
    static bool is_rendering_thread();
    static void set_rendering_thread(bool is_rendering_thread);
    static RenderDevice &instance();
    static RenderDevice *instance_ptr();
    static void set_instance(RenderDevice *device);
    std::mutex &render_loop_mtx() {
        return (_main_stream_ptr == &_async_stream) ? _async_compute_loop_mtx : _render_loop_mtx;
    };
    std::mutex &async_compute_loop_mtx() { return _async_compute_loop_mtx; };
    void set_main_stream(Stream *main_stream);
    void _dispose_io_service();
private:
    // context
    vstd::optional<luisa::compute::Context> _context;
    bool _headless;

    // device
    luisa::compute::Device _device;
    luisa::compute::Device _managed_device;
    luisa::compute::DeviceConfigExt *_device_ext;

    // stream
    luisa::compute::Stream *_main_stream_ptr{};

    // command list
    luisa::compute::CommandList _main_cmd_list;

    // io service
    luisa::compute::Stream _async_copy_stream;
    luisa::compute::Stream _async_stream;
    luisa::compute::TimelineEvent _async_timeline;
    luisa::string _backend;
    IOService *_io_service = nullptr;
    IOService *_mem_io_service = nullptr;
    IOService *_fallback_mem_io_service = nullptr;
    std::mutex _render_loop_mtx;
    std::mutex _async_compute_loop_mtx;
    size_t _device_index;
};

}// namespace rbc