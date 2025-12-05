#pragma once

#include <luisa/runtime/device.h>
#include <luisa/core/platform.h>
#include <luisa/backends/ext/denoiser_ext.h>
#include <luisa/core/dll_export.h>

#include <luisa/backends/ext/native_resource_ext.hpp>
#include <luisa/vstl/meta_lib.h>

namespace rbc {
using namespace luisa::compute;
struct Denoiser;
struct DenoiserExt : public vstd::IOperatorNewBase {
    enum struct Tag : uint32_t {
        CPU,
        DX_VK_Interope,
    };
    virtual Tag tag() const = 0;
    virtual ~DenoiserExt() noexcept = default;

    enum struct PrefilterMode : uint32_t {
        NONE,
        FAST,
        ACCURATE
    };
    enum struct FilterQuality : uint32_t {
        DEFAULT,
        FAST,
        ACCURATE
    };
    enum struct ImageFormat : uint32_t {
        FLOAT1,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        HALF1,
        HALF2,
        HALF3,
        HALF4,
    };
    static constexpr size_t size(ImageFormat fmt) {
        switch (fmt) {
            case ImageFormat::FLOAT1:
                return 4;
            case ImageFormat::FLOAT2:
                return 8;
            case ImageFormat::FLOAT3:
                return 12;
            case ImageFormat::FLOAT4:
                return 16;
            case ImageFormat::HALF1:
                return 2;
            case ImageFormat::HALF2:
                return 4;
            case ImageFormat::HALF3:
                return 6;
            case ImageFormat::HALF4:
                return 8;
            default:
                return 0;
        }
    }
    enum struct ImageColorSpace : uint32_t {
        HDR,
        LDR_LINEAR,
        LDR_SRGB
    };
    struct Image {
        size_t offset{};
        size_t pixel_stride{};
        size_t row_stride{};
        size_t size_bytes{};
        mutable void *out_device_ptr = nullptr;
        ImageFormat format = ImageFormat::FLOAT4;
        ImageColorSpace color_space = ImageColorSpace::HDR;
        float input_scale = 1.0f;
    };
    struct Feature {
        luisa::string name;
        Image image;
    };

    struct DenoiserInput {
        luisa::vector<Image> inputs;
        luisa::vector<Image> outputs;
        // if prefilter is enabled, the feature images might be prefiltered **in-place**
        luisa::vector<Feature> features;
        PrefilterMode prefilter_mode = PrefilterMode::NONE;
        FilterQuality filter_quality = FilterQuality::DEFAULT;
        bool noisy_features = false;
        uint32_t width = 0u;
        uint32_t height = 0u;

    private:
        Image create_image(ImageFormat format, ImageColorSpace cs, float input_scale) {
            auto ele_size = size(format);
            return Image{
                0,
                ele_size,
                ele_size * width,
                ele_size * width * height,
                nullptr,
                format,
                cs,
                input_scale};
        }

    public:
        DenoiserInput() noexcept = delete;
        DenoiserInput(uint32_t width, uint32_t height) noexcept
            : width{width}, height{height} {}
        void push_noisy_image(ImageFormat format,
                              ImageColorSpace cs = ImageColorSpace::HDR,
                              float input_scale = 1.0f) noexcept {
            inputs.push_back(create_image(format, cs, input_scale));
            outputs.push_back(create_image(format, cs, input_scale));
        }
        void push_feature_image(luisa::string feature_name,
                                ImageFormat format,
                                ImageColorSpace cs = ImageColorSpace::HDR,
                                float input_scale = 1.0f) noexcept {
            features.push_back(Feature{
                std::move(feature_name),
                create_image(format, cs, input_scale)});
        }
    };
    virtual luisa::shared_ptr<Denoiser> create() noexcept = 0;
};

template<DenoiserExt::Tag DeriveTag>
struct DenoiserExtDerive : public DenoiserExt {
    DenoiserExt::Tag tag() const override final {
        return DeriveTag;
    }
};

struct Denoiser : public luisa::enable_shared_from_this<Denoiser> {
    virtual void init(DenoiserExt::DenoiserInput const &input) noexcept = 0;
    virtual void execute(bool async) noexcept = 0;
    void execute() noexcept { execute(false); }
    virtual ~Denoiser() noexcept = default;
};

struct DXOidnDenoiserExt : public DenoiserExtDerive<DenoiserExt::Tag::DX_VK_Interope> {
private:
    Device const &_device;
    NativeResourceExt *_native_res_ext;

public:
    template<typename T>
    Buffer<T> buffer_from_image(Image const &image) {
        LUISA_ASSERT(image.out_device_ptr, "Device ptr nullptr.");
        uint state = 0;
        return _native_res_ext->template create_native_buffer<T>(image.out_device_ptr, image.size_bytes / sizeof(T), &state);
    }
    explicit DXOidnDenoiserExt(Device const &device) noexcept;
    luisa::shared_ptr<Denoiser> create() noexcept override;
};
#if defined(_MSC_VER) && !defined(GAME_MODULE_STATIC)
#ifdef OIDN_EXPORT
#define OIDN_API LUISA_EXPORT_API
#else
#define OIDN_API LUISA_IMPORT_API
#endif
#else
#define OIDN_API
#endif

}// namespace rbc
OIDN_API rbc::DenoiserExt *rbc_create_oidn(luisa::compute::Device const &device);
