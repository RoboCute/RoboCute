#pragma once
#include <rbc_config.h>
#include <luisa/core/mathematics.h>
#include <luisa/core/fiber.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/spin_mutex.h>
#include <luisa/vstl/pool.h>
namespace rbc {
using namespace luisa;
class RBC_RUNTIME_API BVH {
public:
	struct Bounding {
		float3 min;
		float3 max;
	};
	template<typename T>
	struct alignas(16) PackedFloat3 {
		std::array<float, 3> xyz;
		T t;
	};
	struct PackedNode {
		// min and index
		std::array<float, 3> min_v;
		uint index;
		// max and lum
		std::array<float, 3> max_v;
		float lum;
		// cone
		float4 cone;
		bool is_leaf() const {
			return (index >> 29u) != 7;
		}
	};
	struct InputNode {
		Bounding bounding;
		float4 bary_center_and_weight;
		float4 cone;
		uint index;
		void write_index(
			uint type,
			uint light_index);
	};

private:
	struct Element {
		float3 min;
		float3 max;
		float3 bary_center;
		float4 cone;
		size_t index;
		float weight;
	};
	struct Node : public vstd::IOperatorNewBase {
		Bounding bounding;
		float3 bary_center;
		float weight;
		uint index;
		float4 cone;
		vstd::variant<Node*, Element*> left_node;
		vstd::variant<Node*, Element*> right_node;
	};
	enum class Axis {
		X,
		Y,
		Z,
		None
	};
	struct Volume {
		float4 left_max_and_score;
		float4 right_min_and_score;
	};
	struct ThreadData {
		std::atomic_uint node_idx_counter;
		vstd::Pool<Node, true> node_pool;
		vstd::vector<Node*> node_array;
		luisa::spin_mutex pool_mtx;
		luisa::spin_mutex node_arr_mtx;
		luisa::fiber::counter fiber_counter;
	};
	static Node* split(
		ThreadData& thread_data,
		vstd::span<Element> input,
		vstd::span<Volume> volume_mem,
		Bounding const& bounding,
		Axis already_sorted_axis,
		uint thread_threshold);

public:
    struct Result {
		Bounding bound;
		float4 cone;
	};
	static Result build(
		vstd::vector<PackedNode>& result,
		vstd::span<InputNode const> boundings);
};
}// namespace rbc