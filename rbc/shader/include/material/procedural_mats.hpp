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
#include <material/gs_mat_impl.hpp>
using namespace luisa::shader;
namespace material {

inline bool procedural_transform_to_params(
    BindlessBuffer &buffer_heap,
    BindlessImage &image_heap,
    ProceduralID procedural_id,
    auto &params,
    float3 input_dir,
    float3 world_pos,
    bool &reject,
    auto &&...vars) {
    // TODO procedural support, need a special material type.
    // Something like this:
    if (procedural_id.mat_heap_id() == ((1u << 28u) - 1)) {
        if constexpr (requires { params.base; }) {
            params.base.color = float3(1, 1, 1);
        }
        return true;
    } else {
        return OpenPBRParticle::transform_to_params(
            buffer_heap,
            image_heap,
            procedural_id.mat_heap_id(),
            procedural_id.mat_idx,
            procedural_id.mat_offset,
            params,
            input_dir,
            reject,
            world_pos, vars...
        );
    }

}
}// namespace material