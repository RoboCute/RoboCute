#pragma once
#include <luisa/core/stl/vector.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/shader.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/bindless_allocator.h>
#include <rbc_render/pipeline_settings.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct Bloom {
	struct Level {
		Image<float> down;
		Image<float> up;
	};
	Bloom();
	luisa::fixed_vector<Level, 16> levels;
	Shader2D<
		Image<float>,// & out_tex,
		Image<float>,
		float, // params,
		float4,//threshold
		Buffer<float>> const* prefilter_shader;
	Shader2D<
		Image<float>,//& out_tex,
		Image<float>> const* downsample_shader;
	Shader2D<
		Image<float>,//& out_tex,
		Image<float>,//& main_tex,
		Image<float>,//& bloom,
		float		 //sample_scale
		> const* upsample_shader;
	struct Result {
		Image<float> bloom_tex;
		float4 dirtTileOffset;
		float4 shaderSettings;
		float4 linearColor;
	};

	Result update(
		BloomSettings const& desc,
		CommandList& cmdlist,
		ImageView<float> read_tex,
		PixelStorage read_tex_storage,
		Buffer<float> const& exposure_buffer,
		uint2 resolution,
		uint2 dirt_tex_size);
};
}// namespace rbc