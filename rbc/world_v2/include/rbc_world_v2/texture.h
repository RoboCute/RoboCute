#pragma once
#include <rbc_world_v2/base_object.h>
#include <rbc_runtime/generated/resource_meta.hpp>
namespace rbc {
struct DeviceImage;
}// namespace rbc
namespace rbc::world {
struct Texture : BaseObjectDerive<Texture, BaseObjectType::Resource> {
protected:
    RC<DeviceImage> _device_image;
    // meta
    luisa::filesystem::path _path;
    LCPixelStorage _pixel_storage;
    luisa::uint2 _size;
    uint64_t _file_offset{};
    uint32_t _mip_level{};

public:
    [[nodiscard]] auto const &path() const { return _path; }
    [[nodiscard]] auto file_offset() const { return _file_offset; }
    [[nodiscard]] auto pixel_storage() const { return _pixel_storage; }
    [[nodiscard]] auto size() const { return _size; }
    [[nodiscard]] auto mip_level() const { return _mip_level; }
    [[nodiscard]] virtual luisa::vector<std::byte> host_data() = 0;
    virtual uint64_t desire_size_bytes() = 0;
    virtual void create_empty(
        luisa::filesystem::path &&path,
        uint64_t file_offset,
        LCPixelStorage pixel_storage,
        luisa::uint2 size,
        uint32_t mip_level) = 0;

    [[nodiscard]] auto device_image() const {
        return _device_image.get();
    }
    virtual bool async_load_from_file() = 0;
    virtual void wait_load() const = 0;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Texture)