#define OIDN_EXPORT
#include <oidn.hpp>
#include <oidn_denoiser.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/stream.h>
#include <luisa/backends/ext/dx_custom_cmd.h>
#include <shared_mutex>
// #include <dxcuda_interop/interop_ext.h>
#include <luisa/backends/ext/dx_cuda_interop.h>
#include <luisa/backends/ext/vk_cuda_interop.h>
#include <luisa/runtime/rhi/device_interface.h>
#include <luisa/runtime/context.h>
#include <luisa/backends/ext/cuda_config_ext.h>
#include <luisa/backends/ext/cuda_external_ext.h>
#include <rbc_graphics/compute_device.h>
namespace rbc {
namespace oidn_detail {
struct CudaDeviceConfigExtImpl : public CUDADeviceConfigExt {
    ExternalVkDevice external_device;
    [[nodiscard]] ExternalVkDevice get_external_vk_device() const noexcept override {
        return external_device;
    }
    CudaDeviceConfigExtImpl(ExternalVkDevice &&external_device) : external_device(std::move(external_device)) {}
};
}// namespace oidn_detail
struct OidnDenoiser : public Denoiser {
protected:
    DeviceInterface *_device;
    oidn::DeviceRef _oidn_device;
    // shared access on ::execute
    // exclusive access on ::init
    std::shared_mutex _mutex;
    luisa::vector<oidn::FilterRef> _filters;
    luisa::vector<oidn::BufferRef> _input_buffers, _output_buffers;
    oidn::BufferRef _albedo_buffer;
    oidn::BufferRef _normal_buffer;
    oidn::FilterRef _albedo_prefilter;
    oidn::FilterRef _normal_prefilter;
    virtual void reset() noexcept override;
    virtual oidn::BufferRef get_buffer(DenoiserExt::Image const &img, bool read) noexcept = 0;
    void exec_filters() noexcept;
    virtual ~OidnDenoiser() = default;

public:
    explicit OidnDenoiser(DeviceInterface *device, oidn::DeviceRef &&oidn_device) noexcept;
    void init(DenoiserExt::DenoiserInput const &input) noexcept override;
};

struct DXOidnDenoiser : public OidnDenoiser {
    // InteropExt _interop;
    DxCudaInterop *_dx_interop_ext;
    VkCudaInterop *_vk_interop_ext;
    struct InteropImage {
        Buffer<uint> shared_buffer;
        bool read;
    };
    luisa::vector<InteropImage> _interop_images;
    oidn::BufferRef get_buffer(DenoiserExt::Image const &img, bool read) noexcept override;
    void reset() noexcept override;
    void async_execute(Stream &render_stream) noexcept override;
    void sync_execute() noexcept override;

public:
    DXOidnDenoiser(Device const &device, int cuda_device_index, oidn::DeviceRef &&oidn_device);
};

OidnDenoiser::OidnDenoiser(DeviceInterface *device, oidn::DeviceRef &&oidn_device) noexcept
    : _device(device), _oidn_device(std::move(oidn_device)) {
    _oidn_device.setErrorFunction([](void *, oidn::Error err, const char *message) noexcept {
        switch (err) {
            case oidn::Error::None:
                break;
            case oidn::Error::Cancelled:
                LUISA_WARNING_WITH_LOCATION("OIDN denoiser cancelled: {}.", message);
                break;
            default:
                LUISA_ERROR_WITH_LOCATION("OIDN denoiser error {}: `{}`.", magic_enum::enum_name(err), message);
        }
    });
    _oidn_device.commit();
}

void OidnDenoiser::reset() noexcept {
    _filters = {};
    _albedo_prefilter = {};
    _normal_prefilter = {};
    _input_buffers = {};
    _output_buffers = {};
    _albedo_buffer = {};
    _normal_buffer = {};
}
void OidnDenoiser::init(DenoiserExt::DenoiserInput const &input) noexcept {
    std::unique_lock lock{_mutex};

    reset();
    auto get_format = [](DenoiserExt::ImageFormat fmt) noexcept {
        if (fmt == DenoiserExt::ImageFormat::FLOAT1) return oidn::Format::Float;
        if (fmt == DenoiserExt::ImageFormat::FLOAT2) return oidn::Format::Float2;
        if (fmt == DenoiserExt::ImageFormat::FLOAT3) return oidn::Format::Float3;
        if (fmt == DenoiserExt::ImageFormat::FLOAT4) return oidn::Format::Float4;
        if (fmt == DenoiserExt::ImageFormat::HALF1) return oidn::Format::Half;
        if (fmt == DenoiserExt::ImageFormat::HALF2) return oidn::Format::Half2;
        if (fmt == DenoiserExt::ImageFormat::HALF3) return oidn::Format::Half3;
        if (fmt == DenoiserExt::ImageFormat::HALF4) return oidn::Format::Half4;
        LUISA_ERROR_WITH_LOCATION("Invalid image format: {}.", (int)fmt);
    };
    auto set_filter_properties = [&](oidn::FilterRef &filter, const DenoiserExt::Image &image) noexcept {
        switch (image.color_space) {
            case DenoiserExt::ImageColorSpace::HDR:
                filter.set("hdr", true);
                break;
            case DenoiserExt::ImageColorSpace::LDR_LINEAR:
                filter.set("hdr", false);
                filter.set("srgb", false);
                break;
            case DenoiserExt::ImageColorSpace::LDR_SRGB:
                filter.set("hdr", false);
                filter.set("srgb", true);
                break;
            default:
                LUISA_ERROR_WITH_LOCATION("Invalid image color space: {}.", (int)image.color_space);
        }
        if (image.input_scale != 1.0) {
            filter.set("inputScale", image.input_scale);
        }
        if (input.filter_quality == DenoiserExt::FilterQuality::FAST) {
            filter.set("filter", oidn::Quality::Balanced);
        } else if (input.filter_quality == DenoiserExt::FilterQuality::ACCURATE) {
            filter.set("filter", oidn::Quality::High);
        }
    };
    auto set_prefilter_properties = [&](oidn::FilterRef &filter) noexcept {
        if (input.prefilter_mode == DenoiserExt::PrefilterMode::NONE) return;
        if (input.prefilter_mode == DenoiserExt::PrefilterMode::FAST) {
            filter.set("quality", oidn::Quality::Balanced);
        } else if (input.prefilter_mode == DenoiserExt::PrefilterMode::ACCURATE) {
            filter.set("quality", oidn::Quality::High);
        }
    };
    bool has_albedo = false;
    bool has_normal = false;
    const DenoiserExt::Image *albedo_image = nullptr;
    const DenoiserExt::Image *normal_image = nullptr;
    if (input.prefilter_mode != DenoiserExt::PrefilterMode::NONE) {
        for (auto &f : input.features) {
            if (f.name == "albedo") {
                LUISA_ASSERT(!has_albedo, "Albedo feature already set.");
                LUISA_ASSERT(!_albedo_prefilter, "Albedo prefilter already set.");
                _albedo_prefilter = _oidn_device.newFilter("RT");
                _albedo_buffer = get_buffer(f.image, true);
                _albedo_prefilter.setImage("color", _albedo_buffer, get_format(f.image.format), input.width, input.height, 0, f.image.pixel_stride, f.image.row_stride);
                _albedo_prefilter.setImage("output", _albedo_buffer, get_format(f.image.format), input.width, input.height, 0, f.image.pixel_stride, f.image.row_stride);
                has_albedo = true;
                albedo_image = &f.image;
                set_prefilter_properties(_albedo_prefilter);
                _albedo_prefilter.commit();
            } else if (f.name == "normal") {
                LUISA_ASSERT(!has_normal, "Normal feature already set.");
                LUISA_ASSERT(!_normal_prefilter, "Normal prefilter already set.");
                _normal_prefilter = _oidn_device.newFilter("RT");
                _normal_buffer = get_buffer(f.image, true);
                _normal_prefilter.setImage("color", _normal_buffer, get_format(f.image.format), input.width, input.height, 0, f.image.pixel_stride, f.image.row_stride);
                _normal_prefilter.setImage("output", _normal_buffer, get_format(f.image.format), input.width, input.height, 0, f.image.pixel_stride, f.image.row_stride);
                has_normal = true;
                normal_image = &f.image;
                set_prefilter_properties(_normal_prefilter);
                _normal_prefilter.commit();
            } else {
                LUISA_ERROR_WITH_LOCATION("Invalid feature name: {}.", f.name);
            }
        }
    }
    LUISA_ASSERT(!input.inputs.empty(), "Empty input.");
    LUISA_ASSERT(input.inputs.size() == input.outputs.size(), "Input/output count mismatch.");
    for (auto i = 0; i < input.inputs.size(); i++) {
        auto filter = _oidn_device.newFilter("RT");
        auto &in = input.inputs[i];
        auto &out = input.outputs[i];
        auto input_buffer = get_buffer(in, true);
        auto output_buffer = get_buffer(out, false);
        filter.setImage("color", input_buffer, get_format(in.format), input.width, input.height, 0, in.pixel_stride, in.row_stride);
        filter.setImage("output", output_buffer, get_format(out.format), input.width, input.height, 0, out.pixel_stride, out.row_stride);
        if (has_albedo) {
            filter.setImage("albedo", _albedo_buffer, get_format(albedo_image->format), input.width, input.height, 0, albedo_image->pixel_stride, albedo_image->row_stride);
        }
        if (has_normal) {
            filter.setImage("normal", _normal_buffer, get_format(normal_image->format), input.width, input.height, 0, normal_image->pixel_stride, normal_image->row_stride);
        }
        set_filter_properties(filter, in);

        if (input.prefilter_mode != DenoiserExt::PrefilterMode::NONE || !input.noisy_features) {
            filter.set("cleanAux", true);
        }
        filter.commit();
        _filters.emplace_back(std::move(filter));
        _input_buffers.emplace_back(std::move(input_buffer));
        _output_buffers.emplace_back(std::move(output_buffer));
    }
}
void OidnDenoiser::exec_filters() noexcept {
    if (_albedo_prefilter) {
        _albedo_prefilter.executeAsync();
    }
    if (_normal_prefilter) {
        _normal_prefilter.executeAsync();
    }
    for (auto &f : _filters) {
        f.executeAsync();
    }
}
auto DXOidnDenoiser::get_buffer(DenoiserExt::Image const &img, bool read) noexcept -> oidn::BufferRef {
    LUISA_ASSERT(img.out_device_ptr == nullptr, "out_device_ptr must be nullptr");
    // TODO: fix this
    // TODO: don't create shared buffer if given buffer is already shared
    uint64_t cuda_device_ptr, cuda_handle;
    Buffer<uint> buffer;
    if (_dx_interop_ext) {
        buffer = _dx_interop_ext->create_buffer<uint>(img.size_bytes);
        _dx_interop_ext->cuda_buffer(buffer.handle(), &cuda_device_ptr, &cuda_handle);
    } else if (_vk_interop_ext) {
        buffer = _vk_interop_ext->create_buffer<uint>(img.size_bytes);
        _vk_interop_ext->cuda_buffer(buffer.handle(), &cuda_device_ptr, &cuda_handle);
    } else {
        LUISA_ERROR("Unknown backend, interop not allowed.");
    }
    // _interop.cuda_buffer(buffer, &cuda_device_ptr, &cuda_handle);
    auto oidn_buffer = _oidn_device.newBuffer(
        (void *)cuda_device_ptr,
        buffer.size_bytes());
    LUISA_ASSERT(oidn_buffer, "OIDN buffer creation failed.");
    img.out_device_ptr = buffer.native_handle();
    _interop_images.push_back(InteropImage{.shared_buffer = std::move(buffer), .read = read});
    return oidn_buffer;
}
void DXOidnDenoiser::reset() noexcept {
    OidnDenoiser::reset();
    _interop_images.clear();
}
void DXOidnDenoiser::async_execute(Stream &render_stream) noexcept {
    ComputeDevice::instance().render_to_compute_fence(render_stream, nullptr);
    exec_filters();
    ComputeDevice::instance().compute_to_render_fence(nullptr, render_stream);
}
void DXOidnDenoiser::sync_execute() noexcept {
    exec_filters();
    _oidn_device.sync();
}
DXOidnDenoiser::DXOidnDenoiser(Device const &device, int cuda_device_index, oidn::DeviceRef &&oidn_device)
    : OidnDenoiser(static_cast<DeviceInterface *>(_device), std::move(oidn_device)), _dx_interop_ext(device.extension<DxCudaInterop>()), _vk_interop_ext(device.extension<VkCudaInterop>()) {
}
DXOidnDenoiserExt::DXOidnDenoiserExt(Device const &device) noexcept
    : _device{device} {
    _native_res_ext = device.extension<NativeResourceExt>();
    LUISA_ASSERT(_native_res_ext, "Native resource ext not found.");
}

luisa::shared_ptr<Denoiser> DXOidnDenoiserExt::create() noexcept {
    auto dx_interop_ext = _device.extension<DxCudaInterop>();
    if (dx_interop_ext) {
        auto cuda_device = dx_interop_ext->cuda_device_index();
        return luisa::make_shared<DXOidnDenoiser>(_device, cuda_device, oidn::newCUDADevice(cuda_device, nullptr));
    }
    auto vk_interop_ext = _device.extension<VkCudaInterop>();
    if (vk_interop_ext) {
        auto cuda_device = vk_interop_ext->cuda_device_index();
        return luisa::make_shared<DXOidnDenoiser>(_device, cuda_device, oidn::newCUDADevice(cuda_device, nullptr));
    }
}
}// namespace rbc
OIDN_API rbc::DenoiserExt *rbc_create_oidn(luisa::compute::Device const &device) {
    return new rbc::DXOidnDenoiserExt(device);
}
