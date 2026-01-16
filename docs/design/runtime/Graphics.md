# RBC Graphics Layer

Robocute的Graphics层是基于LuisaCompute的渲染器封装

## Types

`geometry::MeshMeta`
- heap_idx
- mutable_heap_idx
- tri_byte_offset
- vertex_count
- ele_mask
- submesh_heap_idx
- normal_mask
- tangent_mask
- uv_mask

`geometry::InstanceInfo`
- MeshMeta mesh
- mat_index
- light_mask_id
- last_vertex_heap_idx
- get_light_mask
- get_light_id

`geometry::Triangle = std::array<uint, 3>`
`geometry::Vertex`
- pos
- normal
- tangent
- uvs

`geometry::RasterElement`
- float4x4 local_to_world_and_inst_id
- 因为表示变换的4x4矩阵(4,4)位置元素没有实际意义，正好用来放instance id

## Device Assets

### DeviceTransformingMesh

在Graphics层面上的动态Mesh
- RC DeviceMesh: _origin_mesh
- MeshManager::MeshData: _render_mesh_data
- _last_vertex 为了motion vector的view
- _last_vertex_heap_idx
- async_load()
- create_from_origin
- copy_from_origin

## MeshManager

- a pool of MeshData
- make_transforming_instance

### MeshData

- submesh_offset
- bbox_request
- pack
- triangle_size
- mutable_stride
- meta
- is_vertex_instance
- build_mesh()


