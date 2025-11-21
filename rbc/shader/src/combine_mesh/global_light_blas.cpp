#include <luisa/std.hpp>
#include <lighting/types.hpp>
using namespace luisa::shader;
using namespace lighting;
[[kernel_1d(128)]] int kernel(
	Buffer<BVHNode>& nodes,
	float4x4 local_to_global) {
	auto id = dispatch_id().x;
	BVHNode node = nodes.read(dispatch_size().x + id);
    auto bd = node.bounding();
    float3 min_val = (local_to_global * float4(bd.min, 1.0f)).xyz;
    float3 max_val = min_val;
    float3 val = (local_to_global * float4(bd.min.xy, bd.max.z, 1.0f)).xyz;
    min_val = min(min_val, val);
    max_val = max(max_val, val);
    val = (local_to_global * float4(bd.min.x, bd.max.yz, 1.0f)).xyz;
    min_val = min(min_val, val);
    max_val = max(max_val, val);
    val = (local_to_global * float4(bd.min.x, bd.max.y, bd.min.z, 1.0f)).xyz;
    min_val = min(min_val, val);
    max_val = max(max_val, val);
    val = (local_to_global * float4(bd.max.xy, bd.min.z, 1.0f)).xyz;
    min_val = min(min_val, val);
    max_val = max(max_val, val);
    val = (local_to_global * float4(bd.max.x, bd.min.yz, 1.0f)).xyz;
    min_val = min(min_val, val);
    max_val = max(max_val, val);
    val = (local_to_global * float4(bd.max.x, bd.min.y, bd.max.z, 1.0f)).xyz;
    min_val = min(min_val, val);
    max_val = max(max_val, val);
    val = (local_to_global * float4(bd.max, 1.0f)).xyz;
    min_val = min(min_val, val);
    max_val = max(max_val, val);
    for(int i = 0; i < 3; ++i){
        node.min_v[i] = min_val[i];
        node.max_v[i] = max_val[i];
    }
    node.cone.xyz = (local_to_global * float4(node.cone.xyz, 0.0f)).xyz;
    nodes.write(id, node);
    return 0;
}