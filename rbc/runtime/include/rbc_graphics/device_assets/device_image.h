#pragma once
#include <rbc_graphics/device_assets/device_resource.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/buffer.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/binary_io.h>
#include <rbc_config.h>
// #include <RBQRuntime/tools/image_decoder.hpp>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API DeviceImage : DeviceResource {
public:
    enum ImageType : uint32_t {
        Float,
        Int,
        UInt,
        None
    };

private:
    vstd::variant<
        Image<float>,
        Image<int>,
        Image<uint>>
        _img;
    uint _heap_idx{~0u};
    static uint _check_size(PixelStorage storage, uint2 size, uint desire_mip, uint64_t file_size);
    template<typename T, typename ErrFunc>
    void _create_img(Image<T> &img, PixelStorage storage, uint2 size, uint mip_level, size_t size_bytes, uint &dst_mip_level, ErrFunc &&err_func);
    void _create_heap_idx();
    template<typename LoadType>
    void _async_load(
        Sampler sampler,
        PixelStorage storage,
        uint2 size,
        uint mip_level,
        ImageType image_type,
        LoadType &&load_type,
        bool copy_to_memory = false);
    luisa::vector<std::byte> _host_data;

public:
    Type resource_type() const override { return Type::Image; }
    [[nodiscard]] auto heap_idx() const { return _heap_idx; }
    [[nodiscard]] ImageType type() const { return static_cast<ImageType>(_img.index()); }
    [[nodiscard]] Image<float> const &get_float_image() const;
    [[nodiscard]] Image<int> const &get_int_image() const;
    [[nodiscard]] Image<uint> const &get_uint_image() const;
    [[nodiscard]] luisa::span<std::byte const> host_data() const override { return _host_data; }
    [[nodiscard]] luisa::span<std::byte> host_data() override { return _host_data; }
    [[nodiscard]] luisa::vector<std::byte> &host_data_ref() { return _host_data; }
    DeviceImage();
    ~DeviceImage();

    template<typename T>
    void create_texture(Device &device, PixelStorage storage, uint2 size, uint mip_level) {
        LUISA_ASSERT(_gpu_load_frame == 0, "Texture already loaded.");
        _gpu_load_frame = std::numeric_limits<uint64_t>::max();
        _img = device.create_image<T>(storage, size, mip_level);
        if constexpr (std::is_same_v<T, float>) {
            _create_heap_idx();
        }
    }
    void async_load_from_file(
        luisa::filesystem::path const &path,
        size_t file_offset,
        Sampler sampler,
        PixelStorage storage,
        uint2 size,
        uint mip_level = 1u,
        ImageType image_type = ImageType::Float,
        bool copy_to_host = false);
    void async_load_from_memory(
        BinaryBlob &&data,
        Sampler sampler,
        PixelStorage storage,
        uint2 size,
        uint mip_level = 1u,
        ImageType image_type = ImageType::Float,
        bool copy_to_host = false);
    void async_load_from_buffer(
        BufferView<uint> buffer,
        Sampler sampler,
        PixelStorage storage,
        uint2 size,
        uint mip_level = 1u,
        ImageType image_type = ImageType::Float);
};
}// namespace rbc