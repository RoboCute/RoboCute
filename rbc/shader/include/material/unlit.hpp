#pragma once

#include <utils/shader_host.hpp>

namespace material {
struct Unlit {
    std::array<float, 3> color{0.0f, 0.0f, 0.0f};
    std::array<float, 2> uv_scale{1.0f, 1.0f};
    std::array<float, 2> uv_offset{0.0f, 0.0f};
    MatImageHandle tex;
    SHADER_CODE(
        static float3 get_emission(
            BindlessBuffer &buffer_heap,
            BindlessImage &image_heap,
            uint mat_type,
            uint mat_index,
            std::array<float2, 4> uvs,
            uint uv_count);
        static bool transform_to_params(
            BindlessBuffer &buffer_heap,
            BindlessImage &image_heap,
            uint mat_type,
            uint mat_index,
            auto &params,
            uint texture_filter,
            vt::VTMeta vt_meta,
            std::array<float2, 4> uv,
            uint uv_count,
            float4 ddxy,
            float3 input_dir,
            bool &reject,
            auto &&...);

    )
};
}// namespace material
