#include <rbc_graphics/bvh.h>
#include <luisa/core/stl/algorithm.h>
#include <rbc_graphics/matrix.h>
#include <luisa/core/logging.h>
namespace rbc {
namespace detail {
static float evaluate_cost(
	float theta_o,
	float phi,
	const BVH::Bounding& b) {
	float M_omega = 2 * pi * (1.0f - cos(theta_o));
	// Return complete cost estimate for _LightBounds_

	float3 d = b.max - b.min;
	float surface_area = 2 * (d.x * d.y + d.x * d.z + d.y * d.z);

	return phi * M_omega * surface_area;
}
}// namespace detail
auto BVH::split(
	ThreadData& thread_data,
	vstd::span<Element> input,
	vstd::span<Volume> volume_mem,
	Bounding const& bounding,
	Axis already_sorted_axis,
	uint thread_threshold) -> Node* {
	Axis sort_axis;
	{
		float3 min_center = input[0].bary_center;
		float3 max_center = min_center;
		for (auto& i : input.subspan(1)) {
			min_center = min(min_center, i.bary_center);
			max_center = max(max_center, i.bary_center);
		}
		float3 extent = max_center - min_center;
		if (extent.x > extent.y) {
			if (extent.x > extent.z) {
				sort_axis = Axis::X;
			} else {
				sort_axis = Axis::Z;
			}
		} else {
			if (extent.y > extent.z) {
				sort_axis = Axis::Y;
			} else {
				sort_axis = Axis::Z;
			}
		}
	}
	auto input_size = input.size();
	auto volume_size = input_size - 1;
	auto volume_size_bytes = volume_size * sizeof(Volume);
	auto sort_axis_value = luisa::to_underlying(sort_axis);
	if (already_sorted_axis != sort_axis) {
		luisa::sort(input.data(), input.data() + input.size(), [sort_axis_value](Element& a, Element& b) {
			return a.bary_center[sort_axis_value] < b.bary_center[sort_axis_value];
		});
	}
	float min_volume = std::numeric_limits<float>::max();
	Bounding min_left_bds;
	Bounding min_right_bds;

	size_t cut_pos{};
	{
		float3 left_max = bounding.min;
		float3 right_min = bounding.max;
		float4 left_cone;
		float4 right_cone;
		float left_weight = 0;
		float right_weight = 0;

		for (auto&& i : vstd::range(volume_size)) {
			auto& left = input[i];
			auto& right = input[volume_size - i];
			if (i == 0) {
				left_cone = left.cone;
				right_cone = right.cone;
			} else {
				left_cone = combine_cone(left_cone, left.cone);
				right_cone = combine_cone(right_cone, right.cone);
			}
			left_weight += left.weight;
			right_weight += right.weight;
			left_max = max(left_max, left.max);
			right_min = min(right_min, right.min);
			auto left_size = left_max - bounding.min;
			auto right_size = bounding.max - right_min;

			double left_volume_size = left_size.x * left_size.y + left_size.x * left_size.z + left_size.y * left_size.z;
			left_volume_size *= left_cone.w;
			left_volume_size = std::max(left_volume_size, 1e-6);
			double right_volume_size = right_size.x * right_size.y + right_size.x * right_size.z + right_size.y * right_size.z;
			right_volume_size *= right_cone.w;
			right_volume_size = std::max(right_volume_size, 1e-6);
			auto& left_volume = volume_mem[i];
			auto& right_volume = volume_mem[volume_size - 1 - i];
			left_volume.left_max_and_score = make_float4(left_max, detail::evaluate_cost(left_cone.w, left_weight, Bounding{bounding.min, left_max}));
			right_volume.right_min_and_score = make_float4(right_min, detail::evaluate_cost(right_cone.w, right_weight, Bounding{right_min, bounding.max}));

			// left_volume.left_size = left_volume_size * double(i + 1);
			// right_volume.right_size = right_volume_size * double(i + 1);
		}
		for (auto&& i : vstd::range(0, volume_size)) {
			auto& volume = volume_mem[i];
			auto score = volume.left_max_and_score.w + volume.right_min_and_score.w;
			score *= std::abs(double(i) - double(volume_size) * 0.5f) / (double(volume_size) * 0.5f) * 0.5f + 0.5f;
			if (score < min_volume) {
				min_left_bds = Bounding{
					bounding.min,
					volume.left_max_and_score.xyz()};
				min_right_bds = Bounding{
					volume.right_min_and_score.xyz(),
					bounding.max};
				min_volume = score;
				cut_pos = i;
			}
		}
	}
	// left split
	Node* node = thread_data.node_pool.create_lock(thread_data.pool_mtx);
	node->index = thread_data.node_idx_counter++;
	{
		std::lock_guard lck{thread_data.node_arr_mtx};
		thread_data.node_array.emplace_back(node);
	}
	node->bounding = bounding;
	auto split_offset = cut_pos + 1;
	bool already_threaded = false;
	if (cut_pos > 0) {
		vstd::span<Element> new_input{input.data(), split_offset};
		vstd::span<Volume> new_volume_mem{volume_mem.data(), new_input.size()};
		if (split_offset < thread_threshold) {
			node->left_node = split(
				thread_data,
				new_input,
				new_volume_mem,
				min_left_bds,
				sort_axis,
				thread_threshold);
		} else {
			already_threaded = true;
			thread_data.fiber_counter.add(1);
			luisa::fiber::schedule([&thread_data, fiber = thread_data.fiber_counter, node, new_input, new_volume_mem, min_left_bds, sort_axis, thread_threshold]() {
				node->left_node = split(
					thread_data,
					new_input,
					new_volume_mem,
					min_left_bds,
					sort_axis,
					thread_threshold);
				fiber.done();
			});
		}
	} else {
		node->left_node = input.data();
	}
	// right split
	if (cut_pos < (volume_size - 1)) {
		vstd::span<Element> new_input{input.data() + split_offset, input_size - split_offset};
		vstd::span<Volume> new_volume_mem{volume_mem.data() + split_offset, new_input.size()};
		if (split_offset < thread_threshold || already_threaded) {
			node->right_node = split(
				thread_data,
				new_input,
				new_volume_mem,
				min_right_bds,
				sort_axis,
				thread_threshold);
		} else {
			thread_data.fiber_counter.add(1);
			luisa::fiber::schedule([&thread_data, fiber = thread_data.fiber_counter, node, new_input, new_volume_mem, min_right_bds, sort_axis, thread_threshold]() {
				node->right_node = split(
					thread_data,
					new_input,
					new_volume_mem,
					min_right_bds,
					sort_axis,
					thread_threshold);
				fiber.done();
			});
		}
	} else {
		node->right_node = &input[input.size() - 1];
	}
	return node;
}
void BVH::InputNode::write_index(
	uint type,
	uint light_index) {
#ifndef NDEBUG
	LUISA_ASSERT(type < 7, "Type must be less than 7.");
#endif
	index = (type << 29u) | light_index;
}
auto BVH::build(
	vstd::vector<PackedNode>& packed_nodes,
	vstd::span<InputNode const> boundings) -> Result {
	packed_nodes.clear();
	if (boundings.empty()) return {};
	float3 min_values{
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
	};
	float3 max_values{
		std::numeric_limits<float>::min(),
		std::numeric_limits<float>::min(),
		std::numeric_limits<float>::min(),
	};
	vstd::vector<Element> elements;
	Bounding root_bounding;
	vstd::push_back_func(elements, boundings.size(), [&](uint i) {
		auto& ele = boundings[i];
		auto& bounding = ele.bounding;
		auto b_min = bounding.min;
		auto b_max = bounding.max;
		auto e = Element{
			.min = b_min,
			.max = b_max,
			.bary_center = ele.bary_center_and_weight.xyz(),
			.cone = ele.cone,
			.index = ele.index,
			.weight = ele.bary_center_and_weight.w};
		for (auto v : vstd::range(3)) {
			min_values[v] = std::min(min_values[v], b_min[v]);
			max_values[v] = std::max(max_values[v], b_max[v]);
		}
		return e;
	});
	if (elements.size() == 1) {
		auto& ele = elements[0];
		auto& v = packed_nodes.emplace_back();
		for (auto i : vstd::range(3)) {
			v.min_v[i] = ele.min[i];
			v.max_v[i] = ele.max[i];
		}
		v.index = ele.index;
		v.lum = ele.weight;
		v.cone = ele.cone;

		auto& empty = packed_nodes.emplace_back();
		memset(&empty, 0, sizeof(PackedNode));
		return Result{
			.bound = Bounding{
		.min = min_values,
		.max = max_values},
		.cone = ele.cone
		};
	}
	ThreadData thd_data{
		.node_idx_counter = 0,
		.node_pool = vstd::Pool<Node, true>{boundings.size(), false}};
	thd_data.node_array.reserve(boundings.size() + 1);
	root_bounding = Bounding{
		.min = min_values,
		.max = max_values};

	vstd::vector<Volume> volumes;
	volumes.push_back_uninitialized(elements.size());
	// TODO
	Node* root_node = split(
		thd_data,
		elements,
		volumes,
		root_bounding,
		Axis::None,
		std::max(boundings.size() / 8ull, 256ull));
	thd_data.fiber_counter.wait();
	packed_nodes.push_back_uninitialized(thd_data.node_idx_counter * 2);
	auto calc_weight = [&](auto&& calc_weight, Node* node) {
		auto func = [&]<typename T>(T t) {
			if constexpr (std::is_same_v<T, Node*>) {
				return calc_weight(calc_weight, t);
			} else {
				return t->weight;
			}
		};
		auto weight = node->left_node.visit_or(0.0f, func) +
					   node->right_node.visit_or(0.0f, func);
		node->weight = weight;
		return weight;
	};
	auto calc_cone = [&](auto&& calc_cone, Node* node) {
		auto func = [&]<typename T>(T t) {
			if constexpr (std::is_same_v<T, Node*>) {
				return calc_cone(calc_cone, t);
			} else {
				return t->cone;
			}
		};
		auto cone = combine_cone(
			node->left_node.visit_or(float4(0, 0, 1, 0), func),
			node->right_node.visit_or(float4(0, 0, 1, 0), func));
		node->cone = cone;
		return cone;
	};
	auto calc_bary = [&](auto&& calc_bary, Node* node) {
		float sum_weight = 0;
		auto func = [&]<typename T>(T t) {
			sum_weight += t->weight;
			if constexpr (std::is_same_v<T, Node*>) {
				return calc_bary(calc_bary, t) * t->weight;
			} else {
				return t->bary_center * t->weight;
			}
		};

		auto bary_center = (node->left_node.visit_or(float3(0), func) +
							node->right_node.visit_or(float3(0), func));
		bary_center /= std::max<float>(sum_weight, 1e-4);
		node->bary_center = bary_center;
		return bary_center;
	};
	calc_weight(calc_weight, root_node);
	calc_bary(calc_bary, root_node);
	calc_cone(calc_cone, root_node);
	for (auto& i : thd_data.node_array) {
		auto idx = i->index * 2;
		auto& left_v = packed_nodes[idx];
		auto& right_v = packed_nodes[idx + 1];
		auto node_idx = [&](auto& sub_mode, auto& min_v, auto& max_v, auto& weight, auto& cone, auto& index) {
			sub_mode.visit([&]<typename T>(T t) {
				if constexpr (std::is_same_v<T, Node*>) {
					index = t->index * 2;
					min_v[0] = t->bounding.min[0];
					min_v[1] = t->bounding.min[1];
					min_v[2] = t->bounding.min[2];
					max_v[0] = t->bounding.max[0];
					max_v[1] = t->bounding.max[1];
					max_v[2] = t->bounding.max[2];
					index = index | (7u << 29u);
				} else {
					min_v[0] = t->min[0];
					min_v[1] = t->min[1];
					min_v[2] = t->min[2];
					max_v[0] = t->max[0];
					max_v[1] = t->max[1];
					max_v[2] = t->max[2];
					index = t->index;
				}
				cone = t->cone;
				weight = t->weight;
			});
		};
		node_idx(i->left_node, left_v.min_v, left_v.max_v, left_v.lum, left_v.cone, left_v.index);
		node_idx(i->right_node, right_v.min_v, right_v.max_v, right_v.lum, right_v.cone, right_v.index);
	}
	return Result{
		.bound = root_bounding,
		.cone = root_node->cone
	};
}
}// namespace rbc