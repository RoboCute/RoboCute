#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
namespace geometry {
using namespace luisa::shader;
}// namespace geometry
#else
#include <luisa/core/basic_types.h>
namespace geometry {
using float3 = luisa::float3;
using float2 = luisa::float2;
using float4 = luisa::float4;
using float4x4 = luisa::float4x4;
}// namespace geometry
#endif
namespace geometry {

struct VoxelSurface {
    uint aabb_buffer_heap_idx;
    uint aabb_buffer_offset;// AABB element idx
    uint mat_buffer_id;
    uint mat_buffer_offset;
};

struct GaussianSplatingGeometry {
    uint buffer_id;
    uint probe_offset;
    uint mat_buffer_offset;
    uint sh_degree;
};

struct SDFMap {
    uint volume_idx;
    uint sample_count;
    uint mat_buffer_id;
    uint mat_buffer_offset;
    float3 uvw_scale;
    float3 uvw_offset;
};
struct HeightMap {
    uint heightmap_idx;
    uint sample_count;
    uint mat_buffer_id;
    uint aabb_buffer_heap_idx;
    uint height_minmax_buffer_idx;
    uint height_minmax_buffer_offset_bytes;
    uint2 block_size;
};
enum ProceduralTypeID {
    Voxel = 0,
    SDF = 1,
    GaussianSplat = 2,
    HeightTerrain = 3
};
struct ProceduralType {
    uint type;
    uint meta_byte_offset;
    // TODO: material indices
};
}// namespace geometry