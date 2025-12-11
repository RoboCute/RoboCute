#pragma once
#include <rbc_world_v2/resource_base.h>
#include <rbc_runtime/generated/resource_meta.hpp>
namespace rbc {
struct DeviceImage;
}// namespace rbc
namespace rbc::world {
struct Texture : ResourceBaseImpl<Texture> {
    friend struct TextureImpl;
    using BaseType = ResourceBaseImpl<Texture>;

private:
    RC<DeviceImage> _device_image;
    // meta
    LCPixelStorage _pixel_storage;
    luisa::uint2 _size;
    uint32_t _mip_level{};
    Texture() = default;
    ~Texture() = default;
public:
    [[nodiscard]] auto pixel_storage() const { return _pixel_storage; }
    [[nodiscard]] auto size() const { return _size; }
    [[nodiscard]] auto mip_level() const { return _mip_level; }
    [[nodiscard]] virtual luisa::vector<std::byte> *host_data() = 0;
    [[nodiscard]] virtual uint64_t desire_size_bytes() = 0;
    [[nodiscard]] virtual uint32_t heap_index() const = 0;
    virtual void create_empty(
        luisa::filesystem::path &&path,
        uint64_t file_offset,
        LCPixelStorage pixel_storage,
        luisa::uint2 size,
        uint32_t mip_level) = 0;

    [[nodiscard]] auto device_image() const {
        return _device_image.get();
    }
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Texture)