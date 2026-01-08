#pragma once
#include <rbc_world/resource_base.h>
#include <rbc_plugin/generated/resource_meta.hpp>

namespace rbc {
struct DeviceResource;
struct DeviceImage;
struct DeviceSparseImage;
}// namespace rbc

namespace rbc::world {
struct VTLoadFlag : RCBase {
    std::atomic_bool finished{false};
};
struct TextureLoader;

struct RBC_RUNTIME_API TextureResource final : ResourceBaseImpl<TextureResource> {
    friend struct TextureLoader;
    DECLARE_WORLD_OBJECT_FRIEND(TextureResource)
    using BaseType = ResourceBaseImpl<TextureResource>;

private:
    RC<DeviceResource> _tex;
    mutable rbc::shared_atomic_mutex _async_mtx;
    // meta
    LCPixelStorage _pixel_storage;
    luisa::uint2 _size;
    uint32_t _mip_level{};
    bool _is_vt{};
    TextureResource();
    ~TextureResource();
    void _pack_to_tile_level(uint level, luisa::span<std::byte const> src, luisa::span<std::byte> dst);
public:
    bool empty() const;
    bool is_vt() const;
    bool pack_to_tile();
    bool decode(luisa::filesystem::path const &path, TextureLoader* tex_loader, uint mip_level, bool virtual_tex);
    [[nodiscard]] DeviceImage *get_image() const;
    [[nodiscard]] DeviceSparseImage *get_sparse_image() const;
    [[nodiscard]] auto pixel_storage() const { return _pixel_storage; }
    [[nodiscard]] auto size() const { return _size; }
    [[nodiscard]] auto mip_level() const { return _mip_level; }
    [[nodiscard]] luisa::vector<std::byte> *host_data();
    [[nodiscard]] uint64_t desire_size_bytes() const;
    [[nodiscard]] uint32_t heap_index() const;
    void create_empty(
        LCPixelStorage pixel_storage,
        luisa::uint2 size,
        uint32_t mip_level,
        bool is_virtual_texture);
    rbc::coroutine _async_load() override;
    bool load_executed() const;
    bool init_device_resource();
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
    static uint desired_mip_level(luisa::uint2 size, uint idx);

protected:
    bool _async_load_from_file();
    bool _load_finished() const;
    bool unsafe_save_to_path() const override;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::TextureResource)