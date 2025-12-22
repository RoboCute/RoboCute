#pragma once
#include <rbc_world_v2/resource_base.h>
#include <rbc_runtime/generated/resource_meta.hpp>

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

struct RBC_WORLD_API TextureResource final : ResourceBaseImpl<TextureResource> {
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
    RC<VTLoadFlag> _vt_finished{};
    bool _is_vt{};
    TextureResource();
    ~TextureResource();
    void _pack_to_tile_level(uint level, luisa::span<std::byte const> src, luisa::span<std::byte> dst);
public:
    bool is_vt() const;
    bool pack_to_tile();
    [[nodiscard]] bool empty() const override;
    [[nodiscard]] DeviceImage *get_image() const;
    [[nodiscard]] DeviceSparseImage *get_sparse_image() const;
    [[nodiscard]] auto pixel_storage() const { return _pixel_storage; }
    [[nodiscard]] auto size() const { return _size; }
    [[nodiscard]] auto mip_level() const { return _mip_level; }
    [[nodiscard]] luisa::vector<std::byte> *host_data();
    [[nodiscard]] uint64_t desire_size_bytes() const;
    [[nodiscard]] uint32_t heap_index() const;
    void create_empty(
        luisa::filesystem::path &&path,
        uint64_t file_offset,
        LCPixelStorage pixel_storage,
        luisa::uint2 size,
        uint32_t mip_level,
        bool is_virtual_texture);

    bool load_finished() const override;
    bool load_executed() const;
    bool init_device_resource();
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
    void dispose() override;
    bool async_load_from_file() override;
    void unload() override;
    void wait_load_executed() const;
    void wait_load_finished() const override;
protected:
    bool unsafe_save_to_path() const override;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::TextureResource)