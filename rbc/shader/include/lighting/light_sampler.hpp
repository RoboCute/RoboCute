#pragma once

#include <lighting/polymorphic.hpp>
#include <luisa/numeric.hpp>
#include <luisa/resources/common_extern.hpp>
#include <sampling/sample_funcs.hpp>

namespace lighting {

using namespace luisa::shader;

class LightSampler {
public:
    LightSampler(
        float3 world_pos,
        float3 normal,
        float3 reflection_direction,
        float roughness,
        bool use_specular,
        uint light_count)
        : _world_pos(world_pos),
          _normal(normal),
          _reflection_direction(reflection_direction),
          _roughness(roughness),
          _light_count(light_count),
          _use_specular(use_specular) {
        invalidate();
    }

    LightSample sample_direction(
        SpectrumArg &spectrum_arg,
        auto &sampler) {
        LightSample result;
        if (!select(sampler)) return result;

        PolymorphicLight::visit(
            _light_type,
            []<class Entry>(
                LightSampler &light_sampler,
                SpectrumArg &spectrum_arg,
                auto &sampler,
                LightSample &result) {
                using Light = typename Entry::type;
                auto light = Light::read(
                    light_sampler.light_index());
                light.sample_direction(
                    light_sampler,
                    spectrum_arg,
                    sampler,
                    result);
            },
            *this,
            spectrum_arg,
            sampler,
            result);
        return result;
    }

    bool select_mesh_primitive(
        MeshLight &mesh_light,
        auto &sampler) {
        while (true) {
            uint light_code;
            if (!select_bvh_leaf(
                    mesh_light.blas_heap_idx,
                    sampler,
                    light_code)) {
                invalidate();
                return false;
            }

            auto selected_type = light_code >> uint(29);
            auto selected_index = decode_index(light_code);
            if (selected_type == LightTypes::Blas) {
                _light_index = selected_index;
                mesh_light = MeshLight::read(selected_index);
                continue;
            }

            _light_type = selected_type;
            _primitive_index = selected_index;
            if (_light_type >= LightTypes::LightCount) {
                return false;
            }
            return true;
        }
    }

    float3 world_pos() const { return _world_pos; }
    float selection_probability() const {
        return _selection_probability;
    }
    uint light_type() const { return _light_type; }
    uint light_index() const { return _light_index; }
    uint primitive_index() const { return _primitive_index; }

private:
    bool select(auto &sampler) {
        invalidate();
        if (_light_count == 0u) return false;

        _selection_probability = 1.0f;
        _node_index = 0u;
        uint light_code;
        if (!select_bvh_leaf(
                heap_indices::light_bvh_heap_idx,
                sampler,
                light_code)) {
            return false;
        }

        auto selected_type = light_code >> uint(29);
        _light_index = decode_index(light_code);
        _primitive_index = _light_index;
        _light_type = selected_type == LightTypes::Blas
                          ? LightTypes::MeshLight
                          : selected_type;
        if (_light_type >= LightTypes::LightCount) {
            return false;
        }
        return true;
    }

    static uint decode_index(uint light_code) {
        return light_code & ((uint(1) << uint(29)) - uint(1));
    }

    void invalidate() {
        _light_type = max_uint32;
        _light_index = max_uint32;
        _primitive_index = max_uint32;
    }

    float contribution(
        Bounding bound,
        float4 cone) const {
        float3 center = (bound.min + bound.max) * 0.5f;
        float3 box_size = bound.size();
        float box_dist = length(box_size);
        float to_bounding_dist = max(
            sampling::sd_box(
                _world_pos - center,
                box_size * 0.5f),
            0.02f);
        float3 point_to_light =
            center + _normal * box_dist * 0.5f - _world_pos;
        auto point_dist_sqr = dot(point_to_light, point_to_light);
        point_to_light /= sqrt(point_dist_sqr);
        float in_sphere =
            1.0f - saturate(dot(point_to_light, _normal));
        in_sphere *= in_sphere;
        in_sphere = 1.0f - in_sphere;
        auto angle = dot(-point_to_light, cone.xyz);
        angle = min(cone.w / max(angle, 1e-4f), 1.0f);
        float angle_importance =
            16384.0f *
            luisa::shader::select(
                angle * angle,
                1.0f,
                cone.w > pi * 0.99f);
        float value =
            angle_importance * in_sphere / to_bounding_dist;
        value *= 1.0f / max(0.02f, point_dist_sqr);
        if (_use_specular) {
            value *= pow(
                saturate(dot(
                    _reflection_direction,
                    point_to_light)),
                1.0f / clamp(_roughness, 0.4f, 0.9f));
        }
        return value;
    }

    bool select_bvh_leaf(
        uint accel_idx,
        auto &sampler,
        uint &light_code) {
        while (true) {
            auto left_node =
                g_buffer_heap.buffer_read<BVHNode>(
                    accel_idx,
                    _node_index);
            auto right_node =
                g_buffer_heap.buffer_read<BVHNode>(
                    accel_idx,
                    _node_index + 1u);
            auto left_bound = left_node.bounding();
            auto right_bound = right_node.bounding();
            auto left_cone = left_node.cone;
            auto right_cone = right_node.cone;
            float left_rate =
                contribution(left_bound, left_cone) * left_node.lum;
            float right_rate;
            if (right_cone.w > 1e-8f) {
                right_rate =
                    contribution(right_bound, right_cone) *
                    right_node.lum;
            } else {
                right_rate = 0.0f;
            }
            auto sum_rate = right_rate + left_rate;
            if (sum_rate < 1e-8f) return false;

            right_rate /= sum_rate;
            if (right_rate > 1e-8f) {
                right_rate = max(right_rate, 0.05f);
            }
            if (left_rate > 1e-8f) {
                right_rate = min(right_rate, 0.95f);
            }
            left_rate = 1.0f - right_rate;
            auto random = sampler.next();
            auto choose_right = random < right_rate;
            auto next_index = luisa::shader::select(
                left_node.index,
                right_node.index,
                choose_right);
            _selection_probability *= luisa::shader::select(
                left_rate,
                right_rate,
                choose_right);
            if ((next_index >> uint(29)) != 7u) {
                light_code = next_index;
                return true;
            }
            _node_index = next_index;
        }
    }

private:
    float3 _world_pos;
    float3 _normal;
    float3 _reflection_direction;
    float _roughness;
    float _selection_probability;
    uint _light_count;
    uint _light_type;
    uint _light_index;
    uint _primitive_index;
    uint _node_index;
    bool _use_specular;
};

}// namespace lighting
