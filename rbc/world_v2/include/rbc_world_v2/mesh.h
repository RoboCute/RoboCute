#pragma once
#include <rbc_world_v2/resource_base.h>
namespace rbc {
struct DeviceMesh;
}// namespace rbc
namespace rbc::world {
struct RBC_WORLD_API Mesh final : ResourceBaseImpl<Mesh> {
    DECLARE_WORLD_OBJECT_FRIEND(Mesh)
    using BaseType = ResourceBaseImpl<Mesh>;
private:
    RC<DeviceMesh> _device_mesh;
    luisa::spin_mutex _async_mtx;
    // meta
    vstd::vector<uint> _submesh_offsets;
    uint32_t _vertex_count{};
    uint32_t _triangle_count{};
    uint32_t _uv_count{};
    bool _contained_normal : 1 {};
    bool _contained_tangent : 1 {};
    Mesh() = default;
    ~Mesh() = default;
public:
    [[nodiscard]] luisa::span<uint const> submesh_offsets() const { return _submesh_offsets; }
    [[nodiscard]] auto vertex_count() const { return _vertex_count; }
    [[nodiscard]] auto triangle_count() const { return _triangle_count; }
    [[nodiscard]] auto uv_count() const { return _uv_count; }
    [[nodiscard]] auto contained_normal() const { return _contained_normal; }
    [[nodiscard]] auto contained_tangent() const { return _contained_tangent; }
    [[nodiscard]] luisa::vector<std::byte> *host_data();
    uint64_t desire_size_bytes();
    void create_empty(
        luisa::filesystem::path &&path,
        luisa::span<uint const> submesh_offsets,
        uint64_t file_offset,
        uint32_t vertex_count,
        uint32_t triangle_count,
        uint32_t uv_count,
        bool contained_normal,
        bool contained_tangent);
    [[nodiscard]] auto &device_mesh() {
        return _device_mesh;
    }
    [[nodiscard]] auto const &device_mesh() const {
        return _device_mesh;
    }
    bool loaded() const override;
    void rbc_objser(rbc::JsonSerializer &ser_obj) const override;
    void rbc_objdeser(rbc::JsonDeSerializer &obj) override;
    void dispose() override;
    bool async_load_from_file() override;
    void unload() override;
    void wait_load() const override;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Mesh)