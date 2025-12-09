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
    luisa::vector<std::byte> _host_data;
    MeshManager::MeshData *_render_mesh_data{};
    template<typename LoadType>
    void _async_load(
        LoadType &&load_type,
        uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
        bool build_mesh, bool calculate_bound,
        uint64_t file_size = ~0ull, bool copy_to_host = false);

public:
    std::atomic_uint tlas_ref_count{};
    Type resource_type() const override { return Type::Mesh; }
    [[nodiscard]] auto mesh_data() const { return _render_mesh_data; }
    [[nodiscard]] luisa::span<std::byte const> host_data() const override { return _host_data; }
    [[nodiscard]] luisa::span<std::byte> host_data() override { return _host_data; }
    [[nodiscard]] luisa::vector<std::byte>& host_data_ref()  { return _host_data; }
    DeviceMesh();
    ~DeviceMesh();
    static uint64_t get_mesh_size(uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count);
    // sync
    void create_mesh(
        CommandList &cmdlist,
        uint vertex_count, bool normal, bool tangent, uint uv_count, uint triangle_count, vstd::vector<uint> &&submesh_triangle_offset);
    void set_bounding_box(luisa::span<AABB const> bounding_box);
    void calculate_bounding_box();
    // async
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