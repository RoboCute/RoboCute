#pragma once
#include <rbc_config.h>
#include <luisa/backends/ext/dx_cuda_timeline_event.h>
#include <luisa/core/spin_mutex.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/volume.h>
#include <luisa/runtime/byte_buffer.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/event.h>
#include <luisa/vstl/common.h>
namespace luisa::compute {
struct Stream;
}// namespace luisa::compute
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct RenderDevice;
struct RBC_RUNTIME_API ComputeDevice {
private:
    vstd::variant<
        DxCudaTimelineEvent,
        TimelineEvent>
        _event;
    uint64_t _event_fence{};
    vstd::variant<
        DxCudaInterop *,
        VkCudaInterop *>
        _ext{};
    vstd::optional<luisa::compute::Context> _lc_ctx;
    Device device;
    luisa::string _compute_backend_name;
    luisa::spin_mutex _render_mtx;
    vstd::HashMap<uint, std::pair<Device, luisa::spin_mutex>> _devices;
    uint _render_device_idx{~0u};
    void _init_render();
    Device *_init_interop_event();

public:
    static ComputeDevice &instance();
    static ComputeDevice *instance_ptr();
    ComputeDevice();
    void init(
        luisa::compute::Context &&ctx,
        luisa::compute::Device &&render_device,
        luisa::string_view compute_backend_name = "cuda"sv);
    ComputeDevice(ComputeDevice const &) = delete;
    ComputeDevice(ComputeDevice &&) = delete;
    ~ComputeDevice();
    Device *get_render_hardware_device();
    Device *get_device(uint32_t device_index);
    uint render_hardware_device_index();
    void compute_to_render_fence(
        void *signalled_cu_stream_ptr,
        Stream &wait_render_stream);
    void render_to_compute_fence(
        Stream &signalled_render_stream,
        void *wait_cu_stream_ptr);
    BufferCreationInfo create_interop_buffer(const Type *element, size_t elem_count);
    ResourceCreationInfo create_interop_texture(
        PixelFormat format, uint dimension,
        uint width, uint height, uint depth,
        uint mipmap_levels, bool simultaneous_access, bool allow_raster_target);
    template<typename T>
    Buffer<T> create_interop_buffer(size_t elem_count)  {
        Buffer<T> b{};
        _init_render();
        if (_render_device_idx == ~0u) return {};
        _ext.visit([&](auto &&t) {
            b = t->template create_buffer<T>(elem_count);
        });
        return b;
    }
    ByteBuffer create_interop_byte_buffer(size_t size_bytes) ;
    template<typename T>
    Image<T> create_interop_image(PixelStorage pixel, uint width, uint height, uint mip_levels = 1u, bool simultaneous_access = false, bool allow_raster_target = false)  {
        Image<T> b;
        _init_render();
        if (_render_device_idx == ~0u) return {};
        _ext.visit([&](auto &&t) -> ByteBuffer {
            b = t->template create_image<T>(pixel, width, height, mip_levels, simultaneous_access, allow_raster_target);
        });
        return b;
    }
    template<typename T>
    Image<T> create_interop_image(PixelStorage pixel, uint2 size, uint mip_levels = 1u, bool simultaneous_access = false, bool allow_raster_target = false)  {
        Image<T> b;
        _init_render();
        if (_render_device_idx == ~0u) return {};
        _ext.visit([&](auto &&t) {
            b = t->template create_image<T>(pixel, size, mip_levels, simultaneous_access, allow_raster_target);
        });
        return b;
    }
    template<typename T>
    Volume<T> create_interop_volume(PixelStorage pixel, uint width, uint height, uint volume, uint mip_levels = 1u, bool simultaneous_access = false, bool allow_raster_target = false)  {
        Image<T> b;
        _init_render();
        if (_render_device_idx == ~0u) return {};
        _ext.visit([&](auto &&t) {
            b = t->template create_volume<T>(pixel, width, height, volume, mip_levels, mip_levels, simultaneous_access, allow_raster_target);
        });
        return b;
    }
    template<typename T>
    Volume<T> create_interop_volume(PixelStorage pixel, uint3 size, uint mip_levels = 1u, bool simultaneous_access = false, bool allow_raster_target = false)  {
        Image<T> b;
        _init_render();
        if (_render_device_idx == ~0u) return {};
        _ext.visit([&](auto &&t) {
            b = t->template create_volume<T>(pixel, size, mip_levels, mip_levels, simultaneous_access, allow_raster_target);
        });
        return b;
    }
};
}// namespace rbc