#include <rbc_graphics/compute_device.h>
#include <luisa/backends/ext/cuda_external_ext.h>
#include <luisa/core/logging.h>
namespace rbc {
namespace compute_device_detail {
struct CudaDeviceConfigExtImpl : public CudaDeviceConfigExt {
    ExternalVkDevice external_device;
    [[nodiscard]] ExternalVkDevice get_external_vk_device() const noexcept override {
        return external_device;
    }
};
}// namespace compute_device_detail
static ComputeDevice *_compute_device_inst_{nullptr};
ComputeDevice &ComputeDevice::instance() {
    return *_compute_device_inst_;
}
ComputeDevice *ComputeDevice::instance_ptr() {
    return _compute_device_inst_;
}
ComputeDevice::ComputeDevice() {
    if (_compute_device_inst_) [[unlikely]] {
        LUISA_ERROR("Compute device must be singleton.");
    }
    _compute_device_inst_ = this;
}
void ComputeDevice::init(
    luisa::compute::Context &&ctx,
    luisa::compute::Device &&render_device,
    luisa::string_view compute_backend_name) {
    _lc_ctx.create(std::move(ctx));
    vstd::reset(device, std::move(render_device));
    _compute_backend_name = compute_backend_name;
}
ComputeDevice::~ComputeDevice() {
    if (_compute_device_inst_ != this) return;
    _compute_device_inst_ = nullptr;
}
void ComputeDevice::_init_render() {
    std::lock_guard lck{_render_mtx};
    if (!device) return;
    if (!_ext.valid()) {
        if (auto dx = device.extension<DxCudaInterop>())
            _ext = dx;
        else if (auto vk = device.extension<VkCudaInterop>())
            _ext = vk;
    }
    if (!_ext.valid()) {
        return;
    }
    if (_render_device_idx == ~0u) {
        _render_device_idx = _ext.visit_or(0u, [&](auto &&a) {
            return (uint)a->cuda_device_index();
        });
    }
}
Device *ComputeDevice::get_render_hardware_device() {
    _init_render();
    if (_render_device_idx == ~0u) return nullptr;
    return get_device(_render_device_idx);
}
Device *ComputeDevice::get_device(uint32_t device_index) {
    _render_mtx.lock();
    auto iter = _devices.emplace(device_index);
    auto &v = iter.value();
    _render_mtx.unlock();
    std::lock_guard lck{v.second};
    if (!v.first) {
        v.first = _lc_ctx->create_device(_compute_backend_name);
    }
    return &v.first;
}
Device *ComputeDevice::_init_interop_event() {
    auto compute_device = get_render_hardware_device();
    LUISA_ASSERT(compute_device);
    std::lock_guard lck{_render_mtx};
    if (_event.valid())
        return compute_device;
    _ext.visit([&]<typename T>(T const &t) {
        if constexpr (std::is_same_v<T, DxCudaInterop *>) {
            _event = t->create_timeline_event();
        } else if constexpr (std::is_same_v<T, VkCudaInterop *>) {
            _event = compute_device->create_timeline_event();
        } else {
            static_assert(luisa::always_false_v<T>, "Unsupported interop.");
        }
    });
    return compute_device;
}

void ComputeDevice::compute_to_render_fence(
    void *signalled_cu_stream_ptr,
    Stream &wait_render_stream) {
    _init_render();
    if (_render_device_idx == ~0u) [[unlikely]] {
        LUISA_ERROR("Invalid render deivce.");
    }
    auto compute_device = _init_interop_event();
    auto idx = ++_event_fence;
    _event.visit([&]<typename T>(T &t) {
        if constexpr (std::is_same_v<T, DxCudaTimelineEvent>) {
            auto dx = _ext.try_get<DxCudaInterop *>();
            if (!dx) [[unlikely]] {
                LUISA_ERROR("Unsupported backend.");
            }
            t.cuda_signal_external(*dx, signalled_cu_stream_ptr, idx);
        } else {
            compute_device->extension<CUDAExternalExt>()->cuda_stream_signal(signalled_cu_stream_ptr, t.handle(), idx);
        }
    });
    _event.visit([&]<typename T>(T &t) {
        if constexpr (std::is_same_v<T, DxCudaTimelineEvent>) {
            wait_render_stream << t.dx_wait(idx);
        } else {
            auto vk = _ext.try_get<VkCudaInterop *>();
            if (!vk) [[unlikely]] {
                LUISA_ERROR("Unsupported backend.");
            }
            wait_render_stream << (*vk)->vk_wait(t, idx);
        }
    });
}
void ComputeDevice::render_to_compute_fence(
    Stream &signalled_render_stream,
    void *wait_cu_stream_ptr) {
    _init_render();
    if (_render_device_idx == ~0u) [[unlikely]] {
        LUISA_ERROR("Invalid render deivce.");
    }
    auto compute_device = _init_interop_event();
    auto idx = ++_event_fence;
    _event.visit([&]<typename T>(T &t) {
        if constexpr (std::is_same_v<T, DxCudaTimelineEvent>) {
            signalled_render_stream << t.dx_signal(idx);
        } else {
            auto vk = _ext.try_get<VkCudaInterop *>();
            if (!vk) [[unlikely]] {
                LUISA_ERROR("Unsupported backend.");
            }
            signalled_render_stream << (*vk)->vk_signal(t, idx);
        }
    });
    _event.visit([&]<typename T>(T &t) {
        if constexpr (std::is_same_v<T, DxCudaTimelineEvent>) {
            auto dx = _ext.try_get<DxCudaInterop *>();
            if (!dx) [[unlikely]] {
                LUISA_ERROR("Unsupported backend.");
            }
            t.cuda_wait_external(*dx, wait_cu_stream_ptr, idx);
        } else {
            compute_device->extension<CUDAExternalExt>()->cuda_stream_wait(wait_cu_stream_ptr, t.handle(), idx);
        }
    });
}
}// namespace rbc