#pragma once
#include <rbc_world_v2/resource_base.h>
namespace rbc {
struct DeviceMesh;
}// namespace rbc
namespace rbc::world {
struct SkinWeight {
    int32_t joint_id;
    float weight;
};
struct RBC_WORLD_API Mesh final : ResourceBaseImpl<Mesh> {
    DECLARE_WORLD_OBJECT_FRIEND(Mesh)
    using BaseType = ResourceBaseImpl<Mesh>;
private:
    RC<DeviceMesh> _device_mesh;
    mutable rbc::shared_atomic_mutex _async_mtx;
    // meta
    vstd::vector<uint> _submesh_offsets;
    uint32_t _vertex_count{};
    uint32_t _triangle_count{};
    uint32_t _uv_count{};
    bool _contained_normal : 1 {};
    bool _contained_tangent : 1 {};
    uint _vertex_color_channels{};
    uint _skinning_weight_count{};
    Mesh();
    ~Mesh();
    // extra_data:
    // array<struct SkinWeight { int32_t joint_id; float weight; }, vertex_size>
    // array<float * _vertex_color_channels, vertex_size>
public:
    bool decode(luisa::filesystem::path const &path);
    [[nodiscard]] luisa::span<SkinWeight const> skin_weights() const;
    [[nodiscard]] luisa::span<float const> vertex_colors() const;
    [[nodiscard]] luisa::span<uint const> submesh_offsets() const { return _submesh_offsets; }
    [[nodiscard]] bool empty() const override;
    [[nodiscard]] auto vertex_count() const { return _vertex_count; }
    [[nodiscard]] auto triangle_count() const { return _triangle_count; }
    [[nodiscard]] auto uv_count() const { return _uv_count; }
    [[nodiscard]] auto contained_normal() const { return _contained_normal; }
    [[nodiscard]] auto contained_tangent() const { return _contained_tangent; }
    [[nodiscard]] auto submesh_count() const { return std::max<size_t>(_submesh_offsets.size(), 1); }
    [[nodiscard]] luisa::vector<std::byte> *host_data();
    uint64_t basic_size_bytes() const;
    uint64_t desire_size_bytes() const;
    void create_empty(
        luisa::filesystem::path &&path,
        luisa::vector<uint> &&submesh_offsets,
        uint64_t file_offset,
        uint32_t vertex_count,
        uint32_t triangle_count,
        uint32_t uv_count,
        bool contained_normal,
        bool contained_tangent,
        uint vertex_color_channels,
        uint skinning_weight_count);
    [[nodiscard]] auto const &device_mesh() const {
        return _device_mesh;
    }
    bool init_device_resource() override;
    bool load_executed() const override;
    bool load_finished() const override;
    void serialize(ObjSerialize const &obj) const override;
    void deserialize(ObjDeSerialize const &obj) override;
    void dispose() override;
    bool async_load_from_file() override;
    void unload() override;
    void wait_load_executed() const override;
    void wait_load_finished() const override;
protected:
    bool unsafe_save_to_path() const override;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Mesh)