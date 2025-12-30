#pragma once
#include <rbc_world/resource_base.h>
#include <rbc_graphics/mesh_manager.h>
#include <luisa/runtime/byte_buffer.h>
namespace rbc {
struct DeviceResource;
struct DeviceMesh;
struct DeviceTransformingMesh;
}// namespace rbc

namespace rbc::world {

struct SkinAttrib {
    uint16_t joint_id;
    float weight;
};
// Forward declarations for friend classes
struct IMeshImporter;

struct RBC_RUNTIME_API MeshResource final : ResourceBaseImpl<MeshResource> {
    DECLARE_WORLD_OBJECT_FRIEND(MeshResource)
    using BaseType = ResourceBaseImpl<MeshResource>;
    struct CustomProperty {
        size_t offset_bytes;
        size_t size_bytes;
        rbc::shared_atomic_mutex mtx;
        luisa::compute::ByteBuffer device_buffer;
    };

private:
    RC<DeviceResource> _device_res;
    mutable rbc::shared_atomic_mutex _async_mtx;

    // meta
    vstd::vector<uint> _submesh_offsets;
    RC<MeshResource> _origin_mesh;
    uint32_t _vertex_count{};
    uint32_t _triangle_count{};
    uint32_t _uv_count{};
    bool _contained_normal : 1 {};
    bool _contained_tangent : 1 {};
    vstd::HashMap<luisa::string, CustomProperty> _custom_properties;

    MeshResource();
    ~MeshResource();
    // extra_data:
    // array<struct SkinAttrib { int16_t joint_id; float weight; }, vertex_size>
    // array<float * _vertex_color_channels, vertex_size>
    void _copy_from_mesh(MeshResource *origin_mesh);
public:
    bool decode(luisa::filesystem::path const &path);
    [[nodiscard]] luisa::span<uint const> submesh_offsets() const { return _submesh_offsets; }
    [[nodiscard]] bool empty() const;
    [[nodiscard]] auto vertex_count() const { return _vertex_count; }
    [[nodiscard]] auto triangle_count() const { return _triangle_count; }
    [[nodiscard]] auto uv_count() const { return _uv_count; }
    [[nodiscard]] auto contained_normal() const { return _contained_normal; }
    [[nodiscard]] auto contained_tangent() const { return _contained_tangent; }
    [[nodiscard]] auto submesh_count() const { return std::max<size_t>(_submesh_offsets.size(), 1); }
    [[nodiscard]] luisa::vector<std::byte> *host_data();
    [[nodiscard]] bool is_transforming_mesh() const;
    [[nodiscard]] MeshResource *origin_mesh() const { return _origin_mesh.get(); }

    uint64_t basic_size_bytes() const;
    uint64_t extra_size_bytes() const;
    uint64_t desire_size_bytes() const;
    void create_empty(
        luisa::filesystem::path &&path,
        luisa::vector<uint> &&submesh_offsets,
        uint64_t file_offset,
        uint32_t vertex_count,
        uint32_t triangle_count,
        uint32_t uv_count,
        bool contained_normal,
        bool contained_tangent);

    // generate runtime morphing instance
    void create_as_morphing_instance(MeshResource *origin_mesh);

    [[nodiscard]] std::pair<CustomProperty &, luisa::span<std::byte>> add_property(luisa::string &&name, size_t size_bytes);
    [[nodiscard]] luisa::span<std::byte> get_property_host(luisa::string_view name);
    [[nodiscard]] luisa::compute::ByteBufferView get_property_buffer(luisa::string_view name);
    [[nodiscard]] luisa::compute::ByteBufferView get_or_create_property_buffer(luisa::string_view name);
    [[nodiscard]] DeviceMesh *device_mesh() const;
    [[nodiscard]] DeviceTransformingMesh *device_transforming_mesh() const;
    [[nodiscard]] MeshManager::MeshData *mesh_data() const;
    bool init_device_resource();
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

protected:
    bool unsafe_save_to_path() const override;
};
}// namespace rbc::world

// Include importer header after MeshResource definition
// This ensures ObjMeshImporter is fully defined when used
#include <rbc_world/importers/mesh_importer_obj.h>

RBC_RTTI(rbc::world::MeshResource)