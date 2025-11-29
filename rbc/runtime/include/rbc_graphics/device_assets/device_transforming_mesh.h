#pragma once
#include <rbc_graphics/device_assets/device_mesh.h>

namespace rbc
{
struct RBC_RUNTIME_API DeviceTransformingMesh  : DeviceResource
{
private:
    RC<DeviceMesh> _origin_mesh;
    MeshManager::MeshData* _render_mesh_data{};
    Buffer<float3> _last_vertex; // for motion vectors
    uint _last_vertex_heap_idx{~0u};

public:
    Type resource_type() const override { return Type::TransformingMesh; }
    [[nodiscard]] auto mesh_data() const { return _render_mesh_data; }
    [[nodiscard]] auto last_vertex() const { return BufferView<float3>{ _last_vertex }; }
    [[nodiscard]] auto last_vertex_heap_idx() const { return _last_vertex_heap_idx; }
    [[nodiscard]] auto& device_mesh() const { return *_origin_mesh; }
    DeviceTransformingMesh();
    ~DeviceTransformingMesh();
    void async_load(RC<DeviceMesh> origin_mesh, bool init_buffer = true, bool init_last_vertex = false);
};
} // namespace rbc