#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc {
struct DeviceMesh;
}// namespace rbc
namespace rbc::world {
struct Mesh : BaseObjectDerive<Mesh, BaseObjectType::Resource> {
protected:
    RC<DeviceMesh> _device_mesh;
    // meta
    luisa::filesystem::path _path;
    vstd::vector<uint> _submesh_offsets;
    uint64_t _file_offset{};
    uint32_t _vertex_count{};
    uint32_t _triangle_count{};
    uint32_t _uv_count{};
    bool _contained_normal : 1 {};
    bool _contained_tangent : 1 {};

public:
    [[nodiscard]] auto const &path() const { return _path; }
    [[nodiscard]] luisa::span<uint const> submesh_offsets() const { return _submesh_offsets; }
    [[nodiscard]] auto file_offset() const { return _file_offset; }
    [[nodiscard]] auto vertex_count() const { return _vertex_count; }
    [[nodiscard]] auto triangle_count() const { return _triangle_count; }
    [[nodiscard]] auto uv_count() const { return _uv_count; }
    [[nodiscard]] auto contained_normal() const { return _contained_normal; }
    [[nodiscard]] auto contained_tangent() const { return _contained_tangent; }
    [[nodiscard]] virtual luisa::vector<std::byte>* host_data() = 0;
    virtual uint64_t desire_size_bytes() = 0;
    virtual void create_empty(
        luisa::filesystem::path &&path,
        luisa::span<uint const> submesh_offsets,
        uint64_t file_offset,
        uint32_t vertex_count,
        uint32_t triangle_count,
        uint32_t uv_count,
        bool contained_normal,
        bool contained_tangent) = 0;
    [[nodiscard]] auto device_mesh() const {
        return _device_mesh.get();
    }
    virtual bool async_load_from_file() = 0;
    virtual void wait_load() const = 0;
    virtual void unload() = 0;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Mesh)