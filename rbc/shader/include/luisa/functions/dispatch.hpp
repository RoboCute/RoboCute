#pragma once
#include "./../attributes.hpp"
#include "./../type_traits.hpp"
#include "./../types/vec.hpp"

namespace luisa::shader {

[[expr("dispatch_id")]] extern uint3 dispatch_id();
[[expr("block_id")]] extern uint3 block_id();
[[expr("thread_id")]] extern uint3 thread_id();
[[expr("dispatch_size")]] extern uint3 dispatch_size();
[[expr("kernel_id")]] extern uint32 kernel_id();
[[expr("kernel_id")]] extern uint32 primitive_id();
[[expr("object_id")]] extern uint32 object_id();
[[expr("barycentrics")]] extern float3 barycentrics();

[[callop("SYNCHRONIZE_BLOCK")]] extern void sync_block();

// raster
[[callop("RASTER_DISCARD")]] extern void discard();
[[callop("RASTER_SET_Z_DEPTH")]] extern void set_z_depth(float value);
[[callop("RASTER_SET_Z_DEPTH_GREATER_EQUAL")]] extern void set_z_depth_greater_equal(float value);
[[callop("RASTER_SET_Z_DEPTH_LESS_EQUAL")]] extern void set_z_depth_less_equal(float value);
[[callop("FLATTEN")]] extern void flatten();	  // try flatten next if-stmt
[[callop("BRANCH")]] extern void branch();		  // try un-flatten next if-stmt
[[callop("FORCE_CASE")]] extern void force_case();// make next switch force-case (no if else)
}// namespace luisa::shader