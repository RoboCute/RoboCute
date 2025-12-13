#pragma once
#include <rbc_world_v2/resource_base.h>
#include <rbc_runtime/generated/resource_meta.hpp>
namespace rbc {
struct DeviceResource;
}// namespace rbc
namespace rbc::world {
struct VTLoadFlag : RCBase {
    std::atomic_bool finished{false};
};
struct RBC_WORLD_API Texture final : ResourceBaseImpl<Texture> {
    DECLARE_WORLD_OBJECT_FRIEND(Texture)
    using BaseType = ResourceBaseImpl<Texture>;

private:
    luisa::spin_mutex _async_mtx;
    RC<DeviceResource> _tex;
    // meta
    LCPixelStorage _pixel_storage;
    luisa::uint2 _size;
    uint32_t _mip_level{};
    RC<VTLoadFlag> _vt_finished{};
    bool _is_vt{};
    Texture();
    ~Texture();
public:
    [[nodiscard]] auto pixel_storage() const { return _pixel_storage; }
    [[nodiscard]] auto size() const { return _size; }
    [[nodiscard]] auto mip_level() const { return _mip_level; }
    [[nodiscard]] luisa::vector<std::byte> *host_data();
    [[nodiscard]] uint64_t desire_size_bytes();
    [[nodiscard]] uint32_t heap_index() const;
    void create_empty(
        luisa::filesystem::path &&path,
        uint64_t file_offset,
        LCPixelStorage pixel_storage,
        luisa::uint2 size,
        uint32_t mip_level,
        bool is_virtual_texture);

    bool loaded() const override;
    void rbc_objser(rbc::JsonSerializer &ser_obj) const override;
    void rbc_objdeser(rbc::JsonDeSerializer &obj) override;
    void dispose() override;
    bool async_load_from_file() override;
    void unload() override;
    void wait_load() const override;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Texture)