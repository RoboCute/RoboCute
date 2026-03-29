#pragma once
#include <luisa/std.hpp>
#include <geometry/procedural_types.hpp>
#include <geometry/types.hpp>
#include <utils/heap_indices.hpp>
#include <std/ex/type_list.hpp>
#include "procedural_common.hpp"
#include "voxel_sampling.hpp"
#include "sdf_sampling.hpp"
#include "gaussian_sampling.hpp"
#include "height_sampling.hpp"

namespace sampling {
using namespace luisa::shader;

using PolymorphicGeometry = stdex::type_list<
    geometry::VoxelSurface,
    geometry::SDFMap,
    geometry::GaussianSplatingGeometry,
    geometry::HeightMap
    // More types
    >;

static bool sample_procedural(
    Ray ray,
    auto hit,
    auto &rng,
    float &hit_dist,
    ProceduralGeometry &geometry) {
    auto user_id = g_accel.instance_user_id(hit.inst);
    geometry::ProceduralType procedural_type = g_buffer_heap.buffer_read<geometry::ProceduralType>(heap_indices::procedural_type_buffer_idx, user_id);
    bool sampled = false;
    return PolymorphicGeometry::visit(procedural_type.type, [&]<typename ins>() {
        using type = ins::type;
        auto prim = g_buffer_heap.template byte_buffer_read<type>(heap_indices::buffer_allocator_heap_index, procedural_type.meta_byte_offset);
        return _sample_procedural(ray, ins::index, hit, rng, hit_dist, geometry, prim);
    });
}
}// namespace sampling