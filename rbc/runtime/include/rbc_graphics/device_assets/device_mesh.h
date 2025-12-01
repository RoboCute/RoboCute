#pragma once
#include <rbc_graphics/device_assets/device_resource.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/buffer.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/binary_io.h>
#include <rbc_config.h>
#include <rbc_graphics/mesh_manager.h>
namespace rbc {
struct RBC_RUNTIME_API DeviceMesh : DeviceResource {
private:
    bool _contained_normal : 1 {};
    bool _contained_tangent : 1 {};
    uint _uv_count{};
    MeshManager::MeshData *_render_mesh_data{};
    template<typename LoadType>
    void _async_load(
        LoadType &&load_type,
        uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
        bool build_mesh, bool calculate_bound,
        uint64_t file_size = ~0ull, bool copy_to_host = false);
    luisa::vector<std::byte> _host_data;

public:
    Type resource_type() const override { return Type::Mesh; }
    [[nodiscard]] auto contained_normal() const { return _contained_normal; }
    [[nodiscard]] auto contained_tangent() const { return _contained_tangent; }
    [[nodiscard]] auto uv_count() const { return _uv_count; }
    [[nodiscard]] auto mesh_data() const { return _render_mesh_data; }
    [[nodiscard]] luisa::span<std::byte const> host_data() const override { return _host_data; }
    DeviceMesh();
    ~DeviceMesh();

    void async_load_from_file(
        luisa::filesystem::path const &path,
        uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
        bool build_mesh = true, bool calculate_bound = false,
        uint64_t file_offset = 0,
        uint64_t file_max_size = ~0ull,
        bool copy_to_host = false);
    void async_load_from_memory(
        BinaryBlob &&data,
        uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
        bool build_mesh = true, bool calculate_bound = false, bool copy_to_host = false);
    void async_load_from_buffer(
        Buffer<uint> &&data,
        uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset, bool calculate_bound = false);
};
}// namespace rbc