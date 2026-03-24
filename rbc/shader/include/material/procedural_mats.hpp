#pragma once
#include <luisa/std.hpp>
#include <geometry/procedural_types.hpp>
#include <geometry/types.hpp>
#include <luisa/resources/buffer_heap_extern.hpp>
#include <luisa/resources/volume_heap_extern.hpp>
#include <utils/heap_indices.hpp>
#include <std/ex/type_list.hpp>
#include <sampling/procedural_sampling.hpp>
#include <material/mat_codes.hpp>
#include <virtual_tex/stream.hpp>
#include <material/mats.hpp>
using namespace luisa::shader;
namespace material {

inline bool procedural_transform_to_params(
    BindlessBuffer &buffer_heap,
    BindlessImage &image_heap,
    ProceduralID procedural_id,
    auto &params,
    uint &texture_filter,
    vt::VTMeta vt_meta,
    float3 input_dir,
    float3 world_pos,
    bool &reject,
    auto &&...vars) {
    // TODO procedural support, need a special material type.
    // Something like this:
    // auto openpebr = buffer_heap.buffer_read<OpenPBR>(procedural_id.user_id(), procedural_id.prim_id);
    if constexpr (requires { params.base; }) {
        sampling::PCGSampler sampler(uint2(procedural_id.user_id(), procedural_id.prim_id));
        params.base.color = sampler.next3f();
    }

    return true;
}
}// namespace material