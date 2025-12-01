#pragma once
#include "types.hpp"
#include <luisa/resources.hpp>
namespace geometry {
using namespace luisa::shader;
constexpr uint TriangleSize = sizeof(Triangle);
#define ONLY_OPAQUE_MASK 127u
struct PosNormal {
    float3 pos;
    float3 normal;
    float4 tangent;
};
struct PosUV {
    float3 pos;
    float2 uv;
};
inline uint get_submesh_idx(BindlessBuffer &heap, uint submesh_heap_idx, uint prim_id) {
    uint submesh_idx = 0;
    if (submesh_heap_idx != max_uint32) {
        submesh_idx = heap.buffer_read<uint>(submesh_heap_idx, prim_id);
    }
    return submesh_idx;
}
inline uint get_submesh_idx(ByteBuffer<> &byte_buffer, uint prim_id) {
    return byte_buffer.byte_read<uint>(sizeof(uint) * prim_id);
}
inline float3 read_pos(ByteBuffer<> &byte_buffer, uint vert_id) {
    return byte_buffer.byte_read<float3>(sizeof(float3) * vert_id);
}
inline void write_pos(ByteBuffer<> &byte_buffer, uint vert_id, float3 pos) {
    byte_buffer.byte_write(sizeof(float3) * vert_id, pos);
}
inline std::array<float3, 3> read_vert_pos(BindlessBuffer &heap, uint prim_id, MeshMeta mesh_meta) {
    std::array<float3, 3> pos;
    auto tri = heap.byte_buffer_read<Triangle>(mesh_meta.heap_idx, mesh_meta.tri_byte_offset + TriangleSize * prim_id);
    for (uint i = 0; i < 3; ++i) {
        pos[i] = heap.buffer_read<float3>(mesh_meta.heap_idx, tri[i]);
    }
    return pos;
}
inline std::array<float3, 3> read_vert_pos(ByteBuffer<> &byte_buffer, uint prim_id, uint tri_byte_offset) {
    std::array<float3, 3> pos;
    auto tri = byte_buffer.byte_read<Triangle>(tri_byte_offset + TriangleSize * prim_id);
    for (uint i = 0; i < 3; ++i) {
        pos[i] = byte_buffer.byte_read<float3>(sizeof(float3) * tri[i]);
    }
    return pos;
}
inline std::array<PosUV, 3> read_vert_pos_uv(
    BindlessBuffer &heap,
    uint prim_id,
    MeshMeta mesh_meta) {
    std::array<PosUV, 3> arr;
    auto tri = heap.byte_buffer_read<Triangle>(mesh_meta.heap_idx, mesh_meta.tri_byte_offset + TriangleSize * prim_id);
    for (uint i = 0; i < 3; ++i) {
        arr[i].pos = heap.buffer_read<float3>(mesh_meta.heap_idx, tri[i]);
    }
    uint offset = 16 * mesh_meta.vertex_count;
    if (mesh_meta.ele_mask & MeshMeta::normal_mask) {
        offset += 16 * mesh_meta.vertex_count;
    }
    if ((mesh_meta.ele_mask & MeshMeta::tangent_mask) != 0) {
        offset += 16 * mesh_meta.vertex_count;
    }
    if (mesh_meta.ele_mask & (MeshMeta::uv_mask == 0u))
        return arr;
    for (uint i = 0; i < 3; ++i) {
        arr[i].uv = heap.byte_buffer_read<float2>(
            mesh_meta.heap_idx, offset + tri[i] * 8);
    }
    return arr;
}
inline std::array<float2, 3> read_vert_uv(
    ByteBuffer<> &byte_buffer,
    uint prim_id,
    uint vertex_count,
    uint tri_byte_offset,
    uint ele_mask) {
    std::array<float2, 3> arr;
    auto tri = byte_buffer.byte_read<Triangle>(tri_byte_offset + TriangleSize * prim_id);
    uint offset = 16 * vertex_count;
    if (ele_mask & MeshMeta::normal_mask) {
        offset += 16 * vertex_count;
    }
    if ((ele_mask & MeshMeta::tangent_mask) != 0) {
        offset += 16 * vertex_count;
    }
    if (ele_mask & (MeshMeta::uv_mask == 0u))
        return arr;
    for (uint i = 0; i < 3; ++i) {
        arr[i] = byte_buffer.byte_read<float2>(
            offset + tri[i] * 8);
    }
    return arr;
}
inline std::array<Vertex, 3> read_vertices(
    BindlessBuffer &heap,
    uint prim_id,
    MeshMeta mesh_meta,
    bool &contained_normal,
    bool &contained_tangent,
    uint &contained_uv,
    Triangle &tri) {
    std::array<Vertex, 3> arr;
    tri = heap.byte_buffer_read<Triangle>(mesh_meta.heap_idx, mesh_meta.tri_byte_offset + TriangleSize * prim_id);
    for (uint i = 0; i < 3; ++i) {
        arr[i].pos = heap.buffer_read<float3>(mesh_meta.heap_idx, tri[i]);
    }
    uint offset = 16 * mesh_meta.vertex_count;
    if (mesh_meta.ele_mask & MeshMeta::normal_mask) {
        for (uint i = 0; i < 3; ++i) {
            arr[i].normal = heap.byte_buffer_read<float3>(
                mesh_meta.heap_idx, offset + tri[i] * 16);
        }
        offset += 16 * mesh_meta.vertex_count;
        contained_normal = true;
    } else {
        contained_normal = false;
    }
    if ((mesh_meta.ele_mask & MeshMeta::tangent_mask) != 0) {
        for (uint i = 0; i < 3; ++i) {
            arr[i].tangent = heap.byte_buffer_read<float4>(
                mesh_meta.heap_idx, offset + tri[i] * 16);
        }
        offset += 16 * mesh_meta.vertex_count;
        contained_tangent = true;
    } else {
        contained_tangent = false;
    }
    uint uv_idx;
    for (uv_idx = 0; uv_idx < 4; ++uv_idx) {
        if (mesh_meta.ele_mask & ((MeshMeta::uv_mask << uv_idx) == 0u))
            break;
        for (uint i = 0; i < 3; ++i) {
            arr[i].uvs[uv_idx] = heap.byte_buffer_read<float2>(
                mesh_meta.heap_idx, offset + tri[i] * 8);
        }
        offset += 8 * mesh_meta.vertex_count;
    }
    contained_uv = uv_idx;
    return arr;
}
inline std::array<Vertex, 3> read_vertices(
    ByteBuffer<> &byte_buffer,
    uint prim_id,
    uint tri_byte_offset,
    uint vertex_count,
    uint ele_mask,
    bool &contained_normal,
    bool &contained_tangent,
    uint &contained_uv) {
    std::array<Vertex, 3> arr;
    auto tri = byte_buffer.byte_read<Triangle>(tri_byte_offset + TriangleSize * prim_id);
    for (uint i = 0; i < 3; ++i) {
        arr[i].pos = byte_buffer.byte_read<float3>(tri[i] * 16);
    }
    uint offset = 16 * vertex_count;
    if (ele_mask & MeshMeta::normal_mask) {
        for (uint i = 0; i < 3; ++i) {
            arr[i].normal = byte_buffer.byte_read<float3>(offset + tri[i] * 16);
        }
        offset += 16 * vertex_count;
        contained_normal = true;
    } else {
        contained_normal = false;
    }
    if ((ele_mask & MeshMeta::tangent_mask) != 0) {
        for (uint i = 0; i < 3; ++i) {
            arr[i].tangent = byte_buffer.byte_read<float4>(offset + tri[i] * 16);
        }
        offset += 16 * vertex_count;
        contained_tangent = true;
    } else {
        contained_tangent = false;
    }
    uint uv_idx;
    for (uv_idx = 0; uv_idx < 4; ++uv_idx) {
        if (ele_mask & ((MeshMeta::uv_mask << uv_idx) == 0u))
            break;
        for (uint i = 0; i < 3; ++i) {
            arr[i].uvs[uv_idx] = byte_buffer.byte_read<float2>(offset + tri[i] * 8);
        }
        offset += 8 * vertex_count;
    }
    contained_uv = uv_idx;
    return arr;
}
inline void write_pos_normal(
    ByteBuffer<> &byte_buffer,
    uint vert_id,
    uint vertex_count,
    bool contained_normal,
    bool contained_tangent,
    PosNormal pos_normal) {
    byte_buffer.byte_write(16 * vert_id, pos_normal.pos);
    uint offset = 16;
    if (contained_normal) {
        byte_buffer.byte_write(offset * vertex_count + vert_id * 16, pos_normal.normal);
        offset += 16;
    }
    if (contained_tangent) {
        byte_buffer.byte_write(offset * vertex_count + vert_id * 16, pos_normal.tangent);
        offset += 16;
    }
}
inline PosNormal read_pos_normal(
    ByteBuffer<> &byte_buffer,
    uint vert_id,
    uint vertex_count,
    bool contained_normal,
    bool contained_tangent) {
    PosNormal p;
    p.pos = byte_buffer.byte_read<float3>(16 * vert_id);
    uint offset = 16;
    if (contained_normal) {
        p.normal = byte_buffer.byte_read<float3>(offset * vertex_count + vert_id * 16);
        offset += 16;
    }
    if (contained_tangent) {
        p.tangent = byte_buffer.byte_read<float4>(offset * vertex_count + vert_id * 16);
        offset += 16;
    }
    return p;
}
}// namespace geometry