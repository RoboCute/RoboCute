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
using namespace luisa::shader;
namespace material {

inline bool procedural_transform_to_params(
    BindlessBuffer &buffer_heap,
    BindlessImage &image_heap,
    ProceduralID procedural_id,
    MatMeta meta,
    auto& params,
    uint& texture_filter,
    vt::VTMeta vt_meta,
    float3 input_dir,
    float3 world_pos,
    bool &reject,
    auto &&...vars) {
    geometry::ProceduralType procedural_type = g_buffer_heap.buffer_read<geometry::ProceduralType>(heap_indices::procedural_type_buffer_idx, procedural_id.user_id());
    return PolymorphicMaterial::visit(meta.mat_type, [&]<class ins>() {
        using Type = typename ins::type;
        if constexpr (
            requires {
                { Type::procedural_transform_to_params(buffer_heap,
                                                        image_heap,
                                                        ins::index,
                                                        meta.mat_index,
                                                        procedural_type,
                                                        params,
                                                        texture_filter,
                                                        vt_meta,
                                                        input_dir,
                                                        reject,
                                                        world_pos,
                                                        static_cast<decltype(vars)>(vars)...) } -> std::same_as<bool>;
            }) {
            return Type::procedural_transform_to_params(buffer_heap,
                                                        image_heap,
                                                        ins::index,
                                                        meta.mat_index,
                                                        procedural_type,
                                                        params,
                                                        texture_filter,
                                                        vt_meta,
                                                        input_dir,
                                                        reject,
                                                        world_pos,
                                                        static_cast<decltype(vars)>(vars)...);
        } else {
            return false;
        }
    });
}
}// namespace material