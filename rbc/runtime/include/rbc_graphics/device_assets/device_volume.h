#pragma once
#include <rbc_graphics/device_assets/device_resource.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/volume.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/buffer.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/binary_io.h>
#include <rbc_config.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API DeviceVolume : DeviceResource {
public:
    enum VolumeType : uint32_t {
        Float,
        Int,
        UInt,
        None
    };

private:
    vstd::variant<
        Volume<float>,
        Volume<int>,
        Volume<uint>>
        _vol;
    uint _heap_idx{~0u};
    static uint _check_size(PixelStorage storage, uint3 size, uint desire_mip);
    template<typename T, typename ErrFunc>
    void _create_vol(Volume<T> &vol, PixelStorage storage, uint3 size, uint mip_level, uint &dst_mip_level, ErrFunc &&err_func);
    void _create_heap_idx();
    template<typename LoadType>
    void _async_load(
        Sampler sampler,
        PixelStorage storage,
        uint3 size,
        uint mip_level,
        VolumeType volume_type,
        LoadType &&load_type,
        bool copy_to_memory = false);
    luisa::vector<std::byte> _host_data;

public:
    Type resource_type() const override { return Type::Volume; }
    [[nodiscard]] auto heap_idx() const { return _heap_idx; }
    [[nodiscard]] VolumeType type() const { return static_cast<VolumeType>(_vol.index()); }
    [[nodiscard]] Volume<float> const &get_float_volume() const;
    [[nodiscard]] Volume<int> const &get_int_volume() const;
    [[nodiscard]] Volume<uint> const &get_uint_volume() const;
    [[nodiscard]] luisa::span<std::byte const> host_data() const override { return _host_data; }
    [[nodiscard]] luisa::span<std::byte> host_data() override { return _host_data; }
    [[nodiscard]] luisa::vector<std::byte> &host_data_ref() { return _host_data; }
    DeviceVolume();
    ~DeviceVolume();

    template<typename T>
    void create_volume(Device &device, PixelStorage storage, uint3 size, uint mip_level) {
        LUISA_ASSERT(_gpu_load_frame == 0 || !_vol.valid(), "Volume already loaded.");
        _vol = device.create_volume<T>(storage, size, mip_level);
        if constexpr (std::is_same_v<T, float>) {
            _create_heap_idx();
        }
    }
    void async_load_from_file(
        luisa::filesystem::path const &path,
        size_t file_offset,
        Sampler sampler,
        PixelStorage storage,
        uint3 size,
        uint mip_level = 1u,
        VolumeType volume_type = VolumeType::Float,
        bool copy_to_host = false);
    void async_load_from_memory(
        BinaryBlob &&data,
        Sampler sampler,
        PixelStorage storage,
        uint3 size,
        uint mip_level = 1u,
        VolumeType volume_type = VolumeType::Float,
        bool copy_to_host = false);
    void async_load_from_buffer(
        BufferView<uint> buffer,
        Sampler sampler,
        PixelStorage storage,
        uint3 size,
        uint mip_level = 1u,
        VolumeType volume_type = VolumeType::Float);
};
}// namespace rbc
