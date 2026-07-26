#pragma once

#include <bsdfs/base/bsdf.hpp>
#include <fsd/geometry_accel.hpp>
#include <fsd/lut.hpp>
#include <luisa/resources/common_extern.hpp>
#include <sampling/pcg.hpp>
#include <sampling/sample_funcs.hpp>
#include <utils/onb.hpp>

namespace luisa::shader {
extern Accel &g_accel;
extern Accel &g_fsd_accel;
extern fsd::BufferIndices g_fsd_buffer_indices;
}// namespace luisa::shader

namespace mtl {
using namespace luisa::shader;

namespace fsd_detail {
// Keep these generated lobe integrals paired with the FSD runtime LUT.
// Source: scripts/generate_fsd_tables.py, generator version 2.
constexpr float far_field_min_distance_wavelengths = 20.0f;
constexpr uint max_tessellation_depth = 5u;
constexpr uint max_tessellation_segments = 1u << max_tessellation_depth;
constexpr float alpha1_removed_central_power_integral = 0.00461263985f;
constexpr float alpha2_removed_central_power_integral = 0.124590852f;
constexpr float minimum_obstacle_power = 1e-2f;
constexpr float minimum_nonzero_value = 1e-20f;
constexpr float sinc_taylor_threshold = 1e-3f;
constexpr float alpha1_taylor_threshold = 1e-1f;
constexpr float fallback_tangent_length_squared = 1e-14f;
constexpr float exit_edge_min_offset_fraction = 1.0f / 100.0f;
constexpr float exit_edge_max_offset_fraction = 1.0f / 4.0f;
constexpr float exit_plane_depth_radius_fraction = 0.5f;
}// namespace fsd_detail
using namespace fsd_detail;

template<BSDF Sub>
class FreeSpaceDiffractionBSDF {
private:
	static constexpr BSDFFlags diffraction_flags =
		BSDFFlags::SpecularTransmission;

	enum class SampledComponent : uint {
		None,
		Base,
		Diffraction,
	};

public:
	static constexpr BSDFFlags flags =
		Sub::flags | diffraction_flags;

	struct SampledInteraction {
		float3 position = 0.0f;
		float3 normal = 0.0f;
		bool relocated = false;

		explicit operator bool() const {
			return relocated;
		}
	};

private:
	struct DiffractionSample {
		BSDFSample bsdf;
		float3 world_direction = 0.0f;
		float3 exit_point = 0.0f;

		explicit operator bool() const {
			return static_cast<bool>(bsdf) &&
				bsdf.pdf > 0.0f &&
				all(is_finite(world_direction)) &&
				all(is_finite(exit_point));
		}
	};

struct IncidentFrame {
    float3 tangent;
    float3 bitangent;
    float3 incident;

    [[nodiscard]] float3 to_world(float3 local) const {
        return tangent * local.x +
            bitangent * local.y +
            incident * local.z;
    }

	[[nodiscard]] float3 to_local(float3 world) const {
		return float3(
			dot(tangent, world),
			dot(bitangent, world),
			dot(incident, world));
	}
};

struct Edge {
    float2 vector;
    float2 midpoint;
    float2 amplitude_begin;
    float2 amplitude_end;
    float power;
};

struct Construction {
    IncidentFrame frame;
    float wave_number;
    float beam_sigma;
    float search_radius;
    float obstacle_power;
    float proposal_power;

    [[nodiscard]] bool valid() const {
        return obstacle_power >= minimum_obstacle_power &&
            proposal_power > minimum_nonzero_value;
    }
};

struct FieldContribution {
	float2 coherent_field = 0.0f;
	float proposal_intensity = 0.0f;
};

struct Evaluation {
	Throughput throughput;
	float pdf = 0.0f;
};

Construction _construction;
Sub _sub;
Onb _surface_onb;
float3 _interaction_position = 0.0f;
float3 _previous_interaction_position = 0.0f;
float3 _sampled_interaction_position;
bool _interaction_relocation_enabled = false;
float _weight = 0.0f;
float _sub_sample_weight = 1.0f;
float _diffraction_sample_weight = 0.0f;
SampledComponent _sampled_component = SampledComponent::None;
mutable bool _evaluation_cached = false;
mutable float3 _cached_output = 0.0f;
mutable uint _cached_sampling_flags = 0u;
mutable Evaluation _cached_evaluation;

static float2 complex_multiply(float2 lhs, float2 rhs) {
    return float2(
        lhs.x * rhs.x - lhs.y * rhs.y,
        lhs.x * rhs.y + lhs.y * rhs.x);
}

static float2 complex_i_multiply(float2 value) {
    return float2(-value.y, value.x);
}

static float2 complex_polar(float magnitude, float phase) {
    return magnitude * float2(cos(phase), sin(phase));
}

static float sinc(float value) {
    auto absolute_value = abs(value);
    if (absolute_value >= sinc_taylor_threshold) {
        return sin(value) / value;
    }
    auto squared = value * value;
    return 1.0f - squared / 6.0f + squared * squared / 120.0f;
}

static float alpha1(float2 zeta) {
    if (abs(zeta.x) <= minimum_nonzero_value) return 0.0f;
    auto radius_squared = dot(zeta, zeta);
    if (radius_squared <= minimum_nonzero_value) return 0.0f;
    float oscillation_factor;
    if (abs(zeta.x) < alpha1_taylor_threshold) {
        auto squared_x = zeta.x * zeta.x;
        // Series of (cos(x / 2) - sinc(x / 2)) / (2x).
        oscillation_factor = zeta.x * (
            -1.0f / 24.0f +
            squared_x * (1.0f / 960.0f -
                         squared_x / 107520.0f));
    } else {
        auto half_x = 0.5f * zeta.x;
        oscillation_factor =
            (cos(half_x) - sinc(half_x)) / (2.0f * zeta.x);
    }
    return zeta.y / radius_squared * inv_pi * oscillation_factor;
}

static float alpha2(float2 zeta) {
	auto radius_squared = dot(zeta, zeta);
	if (radius_squared <= minimum_nonzero_value) return 0.0f;
	// Eq. (27), evaluated in the analytic x -> 0 limit on the axis.
	if (abs(zeta.x) <= minimum_nonzero_value) {
		return 0.5f * inv_pi / zeta.y;
	}
	return zeta.y / radius_squared * inv_pi *
		sinc(0.5f * zeta.x) * 0.5f;
}

static float removed_central_field_factor(float radius_squared) {
    return sqrt(max(0.0f, 1.0f - exp(-radius_squared / 6.0f)));
}

static IncidentFrame make_incident_frame(
    float3 incident,
    float3 surface_tangent) {
    incident = normalize(incident);
    auto tangent = surface_tangent -
        dot(surface_tangent, incident) * incident;
    auto tangent_length_squared = dot(tangent, tangent);
    if (tangent_length_squared <= fallback_tangent_length_squared) {
        auto helper = abs(incident.x) < 0.9f
            ? float3(1.0f, 0.0f, 0.0f)
            : float3(0.0f, 1.0f, 0.0f);
        tangent = normalize(cross(helper, incident));
    } else {
        tangent *= rsqrt(tangent_length_squared);
    }
    IncidentFrame result;
    result.tangent = tangent;
    result.bitangent = cross(incident, tangent);
    result.incident = incident;
    return result;
}

static float3 triangle_normal(std::array<float3, 3> vertices) {
    return normalize(cross(
        vertices[1] - vertices[0],
        vertices[2] - vertices[0]));
}

static bool is_boundary_for_incident_direction(
    uint2 triangle_ref,
    uint local_edge_index,
    float3 incident) {
    auto adjacency_offset = g_buffer_heap.uniform_idx_buffer_read<uint>(
        g_fsd_buffer_indices.instance_adjacency_offsets,
        triangle_ref.x);
    auto neighbor_primitive = g_buffer_heap.uniform_idx_buffer_read<uint>(
        g_fsd_buffer_indices.edge_adjacency,
        adjacency_offset + triangle_ref.y * 3u + local_edge_index);
    if (neighbor_primitive == max_uint32) return true;
    auto neighbor_vertices = fsd::read_world_triangle(
        g_buffer_heap,
        g_fsd_accel,
        uint2(triangle_ref.x, neighbor_primitive));
    return dot(incident, triangle_normal(neighbor_vertices)) <= 0.0f;
}

static float incident_amplitude(
    float2 projected_position,
    float depth,
    float beam_sigma) {
    auto squared_distance =
        dot(projected_position, projected_position) + depth * depth;
    return exp(-squared_distance /
               (4.0f * beam_sigma * beam_sigma)) /
        (sqrt(2.0f * pi) * beam_sigma);
}

static float triangle_obstacle_power(
    float2 a,
    float2 b,
    float2 c,
    float2 field_a,
    float2 field_b,
    float2 field_c) {
    auto twice_area = abs(
        -a.y * b.x + a.x * b.y + a.y * c.x -
        b.y * c.x - a.x * c.y + b.x * c.y);
    // Eq. (31) integrates |phi_PL|^2, including the depth phase in
    // Re(phi_j * conjugate(phi_l)). For float2 complex values this is dot().
    auto field_sum =
        dot(field_a, field_a) +
        dot(field_b, field_b) +
        dot(field_c, field_c) +
        dot(field_a, field_b) +
        dot(field_a, field_c) +
        dot(field_b, field_c);
    return twice_area * field_sum / 12.0f;
}

static float point_triangle_distance_squared_2d(
    float2 point,
    float2 a,
    float2 b,
    float2 c) {
    auto edge_ab = b - a;
    auto edge_bc = c - b;
    auto edge_ca = a - c;
    auto orientation = edge_ab.x * (c - a).y - edge_ab.y * (c - a).x;
    auto side_ab = edge_ab.x * (point - a).y - edge_ab.y * (point - a).x;
    auto side_bc = edge_bc.x * (point - b).y - edge_bc.y * (point - b).x;
    auto side_ca = edge_ca.x * (point - c).y - edge_ca.y * (point - c).x;
    auto inside = orientation >= 0.0f
        ? (side_ab >= 0.0f && side_bc >= 0.0f && side_ca >= 0.0f)
        : (side_ab <= 0.0f && side_bc <= 0.0f && side_ca <= 0.0f);
    if (inside) return 0.0f;
    return min(
        fsd::point_segment_distance_squared(float3(point, 0.0f), float3(a, 0.0f), float3(b, 0.0f)),
        min(
            fsd::point_segment_distance_squared(float3(point, 0.0f), float3(b, 0.0f), float3(c, 0.0f)),
            fsd::point_segment_distance_squared(float3(point, 0.0f), float3(c, 0.0f), float3(a, 0.0f))));
}

static float integrate_triangle_obstacle_power(
    std::array<float2, 3> projected,
    float3 depth,
    float beam_sigma,
    float wave_number,
    float search_radius) {
    auto longest_edge = max(
        length(projected[1] - projected[0]),
        max(
            length(projected[2] - projected[0]),
            length(projected[2] - projected[1])));
    auto segment_count = min(
        max(uint(ceil(longest_edge / beam_sigma)), 1u),
        max_tessellation_segments);
    auto inverse_segments = rcp(float(segment_count));
    float result = 0.0f;

    auto grid_point = [&](uint i, uint j) {
        auto u = float(i) * inverse_segments;
        auto v = float(j) * inverse_segments;
        auto w = 1.0f - u - v;
        return float3(
            w * projected[0] + u * projected[1] + v * projected[2],
            w * depth.x + u * depth.y + v * depth.z);
    };
    auto add_triangle = [&](float3 p0, float3 p1, float3 p2) {
        if (point_triangle_distance_squared_2d(
                float2(0.0f), p0.xy, p1.xy, p2.xy) >
            search_radius * search_radius) {
            return;
        }
        auto amplitude0 = incident_amplitude(p0.xy, p0.z, beam_sigma);
        auto amplitude1 = incident_amplitude(p1.xy, p1.z, beam_sigma);
        auto amplitude2 = incident_amplitude(p2.xy, p2.z, beam_sigma);
        result += triangle_obstacle_power(
            p0.xy,
            p1.xy,
            p2.xy,
            complex_polar(amplitude0, -wave_number * p0.z),
            complex_polar(amplitude1, -wave_number * p1.z),
            complex_polar(amplitude2, -wave_number * p2.z));
    };

    for (uint i = 0u; i < segment_count; ++i) {
        for (uint j = 0u; j < segment_count - i; ++j) {
            auto p00 = grid_point(i, j);
            auto p10 = grid_point(i + 1u, j);
            auto p01 = grid_point(i, j + 1u);
            add_triangle(p00, p10, p01);
            if (i + j + 1u < segment_count) {
                auto p11 = grid_point(i + 1u, j + 1u);
                add_triangle(p10, p11, p01);
            }
        }
    }
    return result;
}

static Edge make_edge(
    float2 begin,
    float2 end,
    float begin_depth,
    float end_depth,
    float2 triangle_centroid,
    float wave_number,
    float beam_sigma) {
    auto midpoint = 0.5f * (begin + end);
    auto edge_vector = end - begin;
    auto outward = float2(edge_vector.y, -edge_vector.x);
    if (dot(outward, midpoint - triangle_centroid) < 0.0f) {
        auto old_begin = begin;
        begin = end;
        end = old_begin;
        auto old_depth = begin_depth;
        begin_depth = end_depth;
        end_depth = old_depth;
        edge_vector = end - begin;
        midpoint = 0.5f * (begin + end);
    }

    auto begin_amplitude = incident_amplitude(
        begin,
        begin_depth,
        beam_sigma);
    auto end_amplitude = incident_amplitude(
        end,
        end_depth,
        beam_sigma);
    auto complex_begin = complex_polar(
        begin_amplitude,
        -wave_number * begin_depth);
    auto complex_end = complex_polar(
        end_amplitude,
        -wave_number * end_depth);
    auto difference = complex_begin - complex_end;
    auto sum = complex_begin + complex_end;
    auto coefficient_a = dot(difference, difference);
    auto coefficient_b = 0.25f * dot(sum, sum);

    Edge result;
    result.vector = edge_vector;
    result.midpoint = midpoint;
    result.amplitude_begin = complex_begin;
    result.amplitude_end = complex_end;
    result.power = dot(edge_vector, edge_vector) *
        (coefficient_a * alpha1_removed_central_power_integral +
         coefficient_b * alpha2_removed_central_power_integral);
    return result;
}

static bool clip_segment_to_circle(
    float2 begin,
    float2 end,
    float radius,
    float &lower_t,
    float &upper_t) {
    auto direction = end - begin;
    auto direction_length_squared = dot(direction, direction);
    if (direction_length_squared <= minimum_nonzero_value) {
        lower_t = 0.0f;
        upper_t = 1.0f;
        return dot(begin, begin) <= radius * radius;
    }

    auto begin_dot_direction = dot(begin, direction);
    auto discriminant =
        begin_dot_direction * begin_dot_direction -
        direction_length_squared *
            (dot(begin, begin) - radius * radius);
    if (discriminant < 0.0f) {
        lower_t = 0.0f;
        upper_t = 1.0f;
        return dot(begin, begin) <= radius * radius;
    }

    auto root = sqrt(max(discriminant, 0.0f));
    lower_t = max(
        0.0f,
        (-begin_dot_direction - root) / direction_length_squared);
    upper_t = min(
        1.0f,
        (-begin_dot_direction + root) / direction_length_squared);
    return upper_t > lower_t;
}

static void for_each_edge(
    float3 point,
    Construction construction,
    auto &&callback) {
    auto query = g_fsd_accel.query_all(
        fsd::make_query_ray(point, construction.search_radius),
        fsd::geometry_visibility_mask);
    while (query.proceed()) {
        if (!query.is_procedural_candidate()) continue;
        auto candidate = query.procedural_candidate();
        auto triangle_ref = uint2(candidate.inst, candidate.prim);
        if (!fsd::triangle_is_in_radius(
                g_buffer_heap,
                g_fsd_accel,
                triangle_ref,
                point,
                construction.search_radius)) {
            continue;
        }
        auto vertices = fsd::read_world_triangle(
            g_buffer_heap,
            g_fsd_accel,
            triangle_ref);
        if (dot(construction.frame.incident, triangle_normal(vertices)) <= 0.0f) {
            continue;
        }

        std::array<float2, 3> projected;
        float3 depth;
        for (uint vertex = 0u; vertex < 3u; ++vertex) {
            auto offset = vertices[vertex] - point;
            projected[vertex] = float2(
                dot(construction.frame.tangent, offset),
                dot(construction.frame.bitangent, offset));
            depth[vertex] = dot(-construction.frame.incident, offset);
        }
        auto centroid =
            (projected[0] + projected[1] + projected[2]) / 3.0f;

        for (uint local_edge = 0u; local_edge < 3u; ++local_edge) {
            if (!is_boundary_for_incident_direction(
                    triangle_ref,
                    local_edge,
                    construction.frame.incident)) {
                continue;
            }
            uint begin_index = local_edge == 2u ? 1u : 0u;
            uint end_index = local_edge == 0u ? 1u : 2u;
            auto begin = projected[begin_index];
            auto end = projected[end_index];
            auto begin_depth = depth[begin_index];
            auto end_depth = depth[end_index];
            float lower_t;
            float upper_t;
            if (!clip_segment_to_circle(
                    begin,
                    end,
                    construction.search_radius,
                    lower_t,
                    upper_t)) {
                continue;
            }
            auto clipped_begin = lerp(begin, end, lower_t);
            auto clipped_end = lerp(begin, end, upper_t);
            auto clipped_begin_depth = lerp(begin_depth, end_depth, lower_t);
            auto clipped_end_depth = lerp(begin_depth, end_depth, upper_t);
            auto projected_length = length(clipped_end - clipped_begin);
            auto segment_count = min(
                max(uint(ceil(projected_length /
                              construction.beam_sigma)), 1u),
                max_tessellation_segments);
            for (uint segment = 0u; segment < segment_count; ++segment) {
                auto t0 = float(segment) / float(segment_count);
                auto t1 = float(segment + 1u) / float(segment_count);
                auto edge = make_edge(
                    lerp(clipped_begin, clipped_end, t0),
                    lerp(clipped_begin, clipped_end, t1),
                    lerp(clipped_begin_depth, clipped_end_depth, t0),
                    lerp(clipped_begin_depth, clipped_end_depth, t1),
                    centroid,
                    construction.wave_number,
                    construction.beam_sigma);
                if (edge.power > minimum_nonzero_value) {
                    callback(edge);
                }
            }
        }
    }
}

static Construction build_construction(
    float3 point,
    float3 incident,
    float3 surface_tangent,
    float wavelength_nm,
    float previous_segment_length) {
    Construction result;
    result.frame = make_incident_frame(incident, surface_tangent);
    auto wavelength = max(
        wavelength_nm * fsd::nanometers_to_world_scale,
        minimum_nonzero_value);
    result.wave_number = 2.0f * pi / wavelength;
    result.beam_sigma = fsd::sigma_wavelength_factor * wavelength;
    result.search_radius = fsd::query_radius_sigma_factor * result.beam_sigma;
    result.obstacle_power = 0.0f;
    result.proposal_power = 0.0f;
    if (previous_segment_length <
        far_field_min_distance_wavelengths * wavelength) {
        return result;
    }

    auto query = g_fsd_accel.query_all(
        fsd::make_query_ray(point, result.search_radius),
        fsd::geometry_visibility_mask);
    while (query.proceed()) {
        if (!query.is_procedural_candidate()) continue;
        auto candidate = query.procedural_candidate();
        auto triangle_ref = uint2(candidate.inst, candidate.prim);
        if (!fsd::triangle_is_in_radius(
                g_buffer_heap,
                g_fsd_accel,
                triangle_ref,
                point,
                result.search_radius)) {
            continue;
        }
        auto vertices = fsd::read_world_triangle(
            g_buffer_heap,
            g_fsd_accel,
            triangle_ref);
        if (dot(result.frame.incident, triangle_normal(vertices)) <= 0.0f) {
            continue;
        }
        std::array<float2, 3> projected;
        float3 depth;
        for (uint vertex = 0u; vertex < 3u; ++vertex) {
            auto offset = vertices[vertex] - point;
            projected[vertex] = float2(
                dot(result.frame.tangent, offset),
                dot(result.frame.bitangent, offset));
            depth[vertex] = dot(-result.frame.incident, offset);
        }
        result.obstacle_power += integrate_triangle_obstacle_power(
            projected,
            depth,
            result.beam_sigma,
            result.wave_number,
            result.search_radius);
    }

    for_each_edge(
        point,
        result,
        [&](Edge edge) {
            result.proposal_power += edge.power;
        });
    return result;
}

static FieldContribution evaluate_edge(
    Edge edge,
    float wave_number,
    float2 xi) {
    auto edge_length_squared = dot(edge.vector, edge.vector);
    auto perpendicular = float2(edge.vector.y, -edge.vector.x);
    auto zeta = wave_number * float2(
        dot(edge.vector, xi),
        dot(perpendicular, xi));
    auto central_factor = removed_central_field_factor(dot(zeta, zeta));
    auto amplitude_difference =
        edge.amplitude_begin - edge.amplitude_end;
    auto amplitude_sum =
        edge.amplitude_begin + edge.amplitude_end;
    auto alpha1_value = alpha1(zeta);
    auto alpha2_value = alpha2(zeta);
    auto term1 = amplitude_difference * alpha1_value;
    auto term2 = 0.5f * amplitude_sum * alpha2_value;
    auto phase = complex_polar(
        1.0f,
        -wave_number * dot(edge.midpoint, xi));
    auto field_scale =
        wave_number * edge_length_squared * central_factor;

    FieldContribution result;
    result.coherent_field = complex_multiply(
        phase,
        term1 + complex_i_multiply(term2)) * field_scale;
    // The proposal samples alpha1 and alpha2 as a mixture. Its density is
    // their weighted square sum, not the squared coherent edge field.
    result.proposal_intensity = field_scale * field_scale * (
        dot(amplitude_difference, amplitude_difference) *
            alpha1_value * alpha1_value +
        0.25f * dot(amplitude_sum, amplitude_sum) *
            alpha2_value * alpha2_value);
    return result;
}

static float2 inverse_edge_transform(
    Edge edge,
    float wave_number,
    float2 zeta) {
    auto inverse_scale = rcp(
        wave_number * dot(edge.vector, edge.vector));
    return inverse_scale * float2(
        edge.vector.x * zeta.x + edge.vector.y * zeta.y,
        edge.vector.y * zeta.x - edge.vector.x * zeta.y);
}

static float3 exit_point(
    float3 point,
    IncidentFrame frame,
    Edge edge,
    float search_radius,
    float edge_sample,
    float offset_sample) {
    auto begin = edge.midpoint - 0.5f * edge.vector;
    auto end = edge.midpoint + 0.5f * edge.vector;
    auto direction = end - begin;
    auto direction_length_squared = dot(direction, direction);
    float lower_t = 0.0f;
    float upper_t = 1.0f;
    if (direction_length_squared > minimum_nonzero_value) {
        auto begin_dot_direction = dot(begin, direction);
        auto discriminant =
            begin_dot_direction * begin_dot_direction -
            direction_length_squared *
                (dot(begin, begin) - search_radius * search_radius);
        if (discriminant > 0.0f) {
            auto root = sqrt(discriminant);
            lower_t = clamp(
                (-begin_dot_direction - root) /
                    direction_length_squared,
                0.0f,
                1.0f);
            upper_t = clamp(
                (-begin_dot_direction + root) /
                    direction_length_squared,
                0.0f,
                1.0f);
        }
    }
    auto edge_point = lerp(
        begin,
        end,
        lerp(lower_t, upper_t, edge_sample));
    auto outward = float2(edge.vector.y, -edge.vector.x);
    auto outward_length = length(outward);
    if (outward_length > minimum_nonzero_value) {
        auto minimum_offset =
            outward_length * exit_edge_min_offset_fraction;
        auto maximum_offset = min(
            outward_length * exit_edge_max_offset_fraction,
            max(0.0f, 2.0f * search_radius - length(edge_point)));
        minimum_offset = min(minimum_offset, maximum_offset);
        edge_point += outward / outward_length * lerp(
            minimum_offset,
            maximum_offset,
            offset_sample);
    }
    return point + frame.to_world(float3(
        edge_point,
        -search_radius * exit_plane_depth_radius_fraction));
}

FieldContribution evaluate_fields(
	float2 xi) const {
	FieldContribution result;
	auto point = _interaction_position;
	auto construction = _construction;
	for_each_edge(
		point,
		construction,
		[&](Edge edge) {
			auto edge_evaluation = evaluate_edge(
				edge,
				construction.wave_number,
				xi);
			result.coherent_field +=
				edge_evaluation.coherent_field;
			result.proposal_intensity +=
				edge_evaluation.proposal_intensity;
		});
	return result;
}

Evaluation evaluate_direction(
	float3 world_output) const {
	Evaluation result;
	if (!_construction.valid()) return result;

	auto local_output = _construction.frame.to_local(
		normalize(world_output));
	if (local_output.z >= 0.0f) return result;

	// xi is the gnomonic slope used by Eqs. (23)-(40).
	auto xi = local_output.xy / local_output.z;
	auto field = evaluate_fields(xi);
	auto cosine = max(abs(local_output.z), minimum_nonzero_value);
	auto coherent_intensity = dot(
		field.coherent_field,
		field.coherent_field);
	// Throughput stores f * abs(cos(theta)). The cosine in Eq. (38)
	// therefore cancels; only the proposal density needs conversion from
	// gnomonic area using d_omega = cos^3(theta) d_xi.
	auto throughput = coherent_intensity /
		_construction.obstacle_power;
	auto gnomonic_jacobian = cosine * cosine * cosine;
	auto sample_pdf = field.proposal_intensity /
		(_construction.proposal_power * gnomonic_jacobian);
	if (throughput > 0.0f && is_finite(throughput)) {
		result.throughput.val = float3(throughput);
		result.throughput.flags = diffraction_flags;
	}
	if (sample_pdf > 0.0f && is_finite(sample_pdf)) {
		result.pdf = sample_pdf;
	}
	return result;
}

Evaluation evaluate(
	float3 wi,
	float3 wo,
	auto& data) const {
	auto sampling_flags = static_cast<uint>(data.sampling_flags);
	if (_evaluation_cached &&
		_cached_sampling_flags == sampling_flags &&
		all(_cached_output == wo)) {
		return _cached_evaluation;
	}

	Evaluation result;
	if (_construction.valid() &&
		wi.z * wo.z < 0.0f &&
		(data.sampling_flags & diffraction_flags)) {
		result = evaluate_direction(
			_surface_onb.to_world(wo));
	}
	_cached_output = wo;
	_cached_sampling_flags = sampling_flags;
	_cached_evaluation = result;
	_evaluation_cached = true;
	return result;
}

public:
	FreeSpaceDiffractionBSDF() = default;

private:
	[[nodiscard]] bool valid() const {
		return _construction.valid();
	}

	DiffractionSample sample_diffraction(
		auto &sampler) const {
		DiffractionSample result;
		result.exit_point = _interaction_position;
		if (!valid()) return result;

		auto direction_edge_target = sampler.next(g_buffer_heap) *
			_construction.proposal_power;
		auto exit_edge_target = sampler.next(g_buffer_heap) *
			_construction.proposal_power;
		float cumulative_power = 0.0f;
		bool selected_direction_edge = false;
		bool selected_exit_edge = false;
		bool found_edge = false;
		Edge direction_edge;
		Edge exit_edge;
		Edge last_edge;
		for_each_edge(
			_interaction_position,
			_construction,
			[&](Edge edge) {
				found_edge = true;
				last_edge = edge;
				cumulative_power += edge.power;
				if (!selected_direction_edge &&
					direction_edge_target <= cumulative_power) {
					direction_edge = edge;
					selected_direction_edge = true;
				}
				if (!selected_exit_edge &&
					exit_edge_target <= cumulative_power) {
					exit_edge = edge;
					selected_exit_edge = true;
				}
			});
		if (!selected_direction_edge && found_edge) {
			direction_edge = last_edge;
			selected_direction_edge = true;
		}
		if (!selected_exit_edge && found_edge) {
			exit_edge = last_edge;
			selected_exit_edge = true;
		}
		if (!selected_direction_edge || !selected_exit_edge) return result;

		auto amplitude_difference =
			direction_edge.amplitude_begin - direction_edge.amplitude_end;
		auto amplitude_sum =
			direction_edge.amplitude_begin + direction_edge.amplitude_end;
		// The LUTs are individually normalized. Their conditional mixture
		// probabilities must therefore include the respective lobe integrals
		// to reproduce the edge power used when selecting the edge.
		auto alpha1_weight =
			dot(amplitude_difference, amplitude_difference) *
			alpha1_removed_central_power_integral;
		auto alpha2_weight =
			0.25f * dot(amplitude_sum, amplitude_sum) *
			alpha2_removed_central_power_integral;
		auto lobe_weight_sum = alpha1_weight + alpha2_weight;
		if (lobe_weight_sum <= minimum_nonzero_value) return result;

		auto alpha1_lobe =
			sampler.next(g_buffer_heap) * lobe_weight_sum < alpha1_weight;
		auto canonical = fsd::sample_canonical_lobe(
			g_buffer_heap,
			g_fsd_buffer_indices.inverse_cdf_lut,
			alpha1_lobe,
			float3(
				sampler.next2f(g_buffer_heap),
				sampler.next(g_buffer_heap)));
		auto xi = inverse_edge_transform(
			direction_edge,
			_construction.wave_number,
			canonical);
		auto field = evaluate_fields(xi);
		if (field.proposal_intensity <= minimum_nonzero_value) {
			return result;
		}

		auto local_direction = normalize(-float3(xi, 1.0f));
		auto cosine = max(abs(local_direction.z), minimum_nonzero_value);
		auto coherent_intensity = dot(
			field.coherent_field,
			field.coherent_field);
		auto throughput = coherent_intensity /
			_construction.obstacle_power;
		auto gnomonic_jacobian = cosine * cosine * cosine;
		auto sample_pdf = field.proposal_intensity /
			(_construction.proposal_power * gnomonic_jacobian);
		if (!(throughput > 0.0f && sample_pdf > 0.0f) ||
			!is_finite(throughput) || !is_finite(sample_pdf)) {
			return result;
		}

		result.bsdf.wo = local_direction;
		result.bsdf.pdf = sample_pdf;
		result.bsdf.eta = 1.0f;
		result.bsdf.throughput.val = float3(throughput);
		result.bsdf.throughput.flags = diffraction_flags;
		result.world_direction = normalize(
			_construction.frame.to_world(local_direction));
		result.exit_point = exit_point(
			_interaction_position,
			_construction.frame,
			exit_edge,
			_construction.search_radius,
			sampler.next(g_buffer_heap),
			sampler.next(g_buffer_heap));
		if (!static_cast<bool>(result)) {
			result.bsdf.throughput.flags = BSDFFlags::None;
			result.bsdf.pdf = 0.0f;
		}
		return result;
	}

private:
	void init_diffraction(
		float3 wi,
		auto const& basic_parameter,
		auto& data) {
		_evaluation_cached = false;
		_surface_onb = basic_parameter.geometry.onb;
		if (!_interaction_relocation_enabled) {
			_construction.obstacle_power = 0.0f;
			_construction.proposal_power = 0.0f;
			return;
		}
		auto incident = _surface_onb.to_world(wi);
		_construction = build_construction(
			_interaction_position,
			incident,
			_surface_onb.tangent,
			data.lambda[data.hero_wavelength_index],
			length(
				_interaction_position -
					_previous_interaction_position));
	}

	[[nodiscard]] Throughput eval_diffraction(
		float3 wi,
		float3 wo,
		auto& data) const {
		return evaluate(wi, wo, data).throughput;
	}

	[[nodiscard]] float pdf_diffraction(
		float3 wi,
		float3 wo,
		auto& data) const {
		return evaluate(wi, wo, data).pdf;
	}

	BSDFSample sample_diffraction_bsdf(
		float3 wi,
		auto& data) {
		BSDFSample result;
		if (!valid() ||
			!(data.sampling_flags & diffraction_flags)) {
			return result;
		}

		sampling::PCGSampler sampler(uint3(
			bit_cast<uint>(data.rand.x),
			bit_cast<uint>(data.rand.y),
			bit_cast<uint>(data.rand.z)));
		auto diffraction_sample = sample_diffraction(sampler);
		if (!diffraction_sample) return result;

		auto surface_wo =
			_surface_onb.to_local(diffraction_sample.world_direction);
		if (wi.z * surface_wo.z >= 0.0f) return result;

		auto visibility_segment =
			diffraction_sample.exit_point -
			_previous_interaction_position;
		auto visibility_distance = length(visibility_segment);
		if (visibility_distance <= 0.0f) return result;
		if (visibility_distance > sampling::offset_ray_t_min) {
			auto visibility_direction =
				visibility_segment / visibility_distance;
			auto visibility_origin =
				_previous_interaction_position +
					visibility_direction * sampling::offset_ray_t_min;
			auto visibility_length = max(
				0.0f,
				visibility_distance -
					2.0f * sampling::offset_ray_t_min);
			if (g_accel.trace_any(
					Ray(
						visibility_origin,
						visibility_direction,
						0.0f,
						visibility_length),
					fsd::geometry_visibility_mask)) {
				return result;
			}
		}

		result = diffraction_sample.bsdf;
		result.wo = surface_wo;
		_sampled_interaction_position =
			diffraction_sample.exit_point;
		data.selected_wavelength = true;
		return result;
	}

	[[nodiscard]] float3 diffraction_energy(
		auto& data) const {
		return valid() &&
			(data.sampling_flags & diffraction_flags)
			? float3(1.0f)
			: float3(0.0f);
	}

public:
	void prepare_interaction(
		float3 interaction_position,
		float3 previous_interaction_position,
		bool relocation_enabled) {
		_interaction_position = interaction_position;
		_previous_interaction_position =
			previous_interaction_position;
		_interaction_relocation_enabled = relocation_enabled;
	}

	void init(
		float3 wi,
		auto const& basic_parameter,
		auto const& extra_parameter,
		auto& data) {
		_sampled_component = SampledComponent::None;
		_weight = saturate(
			basic_parameter.weight.free_space_diffraction);
		_sub.init(wi, basic_parameter, extra_parameter, data);
		_sub_sample_weight = 1.0f;
		_diffraction_sample_weight = 0.0f;
		if (_weight == 0.0f) return;

		init_diffraction(wi, basic_parameter, data);
		auto sub_energy = _sub.energy(wi, data);
		_sub_sample_weight =
			spectrum::spectrum_to_weight(sub_energy);
		_diffraction_sample_weight =
			_weight * spectrum::spectrum_to_weight(
				diffraction_energy(data));
		auto sum_weight =
			_sub_sample_weight + _diffraction_sample_weight;
		if (sum_weight <= 0.0f) return;
		_sub_sample_weight /= sum_weight;
		_diffraction_sample_weight /= sum_weight;
	}

	Throughput eval(
		float3 wi,
		float3 wo,
		auto& data) const {
		// NEE runs after sample(), so condition on the selected top-level
		// branch while keeping its joint branch PDF for MIS.
		if (_sampled_component == SampledComponent::Diffraction) {
			if (_diffraction_sample_weight <= 0.0f) return {};
			return eval_diffraction(wi, wo, data) *
				(_weight / _diffraction_sample_weight);
		}
		if (_sampled_component == SampledComponent::Base) {
			if (_sub_sample_weight <= 0.0f) return {};
			auto result = _sub.eval(wi, wo, data);
			result *= rcp(_sub_sample_weight);
			return result;
		}

		auto result = _sub.eval(wi, wo, data);
		if (_weight > 0.0f) {
			result += eval_diffraction(wi, wo, data) * _weight;
		}
		return result;
	}

	BSDFSample sample(
		float3 wi,
		auto& data,
		auto& volume_stack) {
		_sampled_component = SampledComponent::None;
		if (_sub_sample_weight +
				_diffraction_sample_weight <=
			0.0f) {
			return {};
		}
		if (data.rand.z < _diffraction_sample_weight) {
			data.rand.z /= _diffraction_sample_weight;
			auto result = sample_diffraction_bsdf(wi, data);
			if (result) {
				_sampled_component =
					SampledComponent::Diffraction;
			}
			result.throughput *= _weight;
			result.pdf *= _diffraction_sample_weight;
			return result;
		}

		data.rand.z =
			(data.rand.z - _diffraction_sample_weight) /
			_sub_sample_weight;
		_sampled_component = SampledComponent::Base;
		auto result = _sub.sample(wi, data, volume_stack);
		result.pdf *= _sub_sample_weight;
		return result;
	}

	float pdf(
		float3 wi,
		float3 wo,
		auto& data) const {
		if (_sampled_component == SampledComponent::Diffraction) {
			return _diffraction_sample_weight > 0.0f
				? pdf_diffraction(wi, wo, data) *
					_diffraction_sample_weight
				: 0.0f;
		}
		if (_sampled_component == SampledComponent::Base) {
			return _sub_sample_weight > 0.0f
				? _sub.pdf(wi, wo, data) *
					_sub_sample_weight
				: 0.0f;
		}

		auto sub_pdf = _sub.pdf(wi, wo, data);
		if (_weight == 0.0f) return sub_pdf;
		if (_sub_sample_weight +
				_diffraction_sample_weight <=
			0.0f) {
			return 0.0f;
		}
		return lerp(
			sub_pdf,
			pdf_diffraction(wi, wo, data),
			_diffraction_sample_weight);
	}

	float3 tint_out(float3 wo, auto& data) const {
		return _sub.tint_out(wo, data);
	}

	float3 trans(
		float3 wi,
		auto& data,
		float3 base_energy) const {
		return _sub.trans(wi, data, base_energy);
	}

	float3 energy(float3 wi, auto& data) const {
		auto result = _sub.energy(wi, data);
		if (_weight == 0.0f) return result;
		return result + diffraction_energy(data) * _weight;
	}

	[[nodiscard]] SampledInteraction sampled_interaction() const {
		SampledInteraction result;
		if (_sampled_component != SampledComponent::Diffraction) {
			return result;
		}
		result.position = _sampled_interaction_position;
		result.normal = -_construction.frame.incident;
		result.relocated = true;
		return result;
	}
};

}// namespace mtl
