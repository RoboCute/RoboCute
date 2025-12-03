#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/shader.h>
#include <luisa/core/fiber.h>
#include <luisa/core/dynamic_module.h>
namespace rbc {
struct AccumPassContext;
struct  AccumPass : public Pass {
public:
	luisa::fiber::counter init_counter;
	// Shader2D<
	// 	Image<float>,// specular
	// 	Image<float>,// diffuse
	// 	Image<float>,// albedo
	// 	Image<float>> const* modulate;
	Shader2D<
		Image<float>,
		Image<float>,
		Image<float>,
		uint2,
		uint> const* accum;
	// ShaderBase const* _lut_baker;
	Buffer<float4> lut_buffer;
	uint64_t buffer_frame_idx = 0;
	AccumPassContext* pass_ctx{};
	Image<float> temp_img;
	void on_enable(
		Pipeline const& pipeline,
		Device& device,
		CommandList& cmdlist,
		SceneManager& scene) override;
	void early_update(Pipeline const& pipeline, PipelineContext const& ctx) override;
	void update(Pipeline const& pipeline, PipelineContext const& ctx) override;
	void on_frame_end(
		Pipeline const& pipeline,
		Device& device,
		SceneManager& scene) override;
	void on_disable(Pipeline const& pipeline,
					Device& device,
					CommandList& cmdlist,
					SceneManager& scene) override;
	void wait_enable() override;
};
struct AccumPassContext : public PassContext {
    Image<float> hdr;
    uint64_t frame_index{};
    AccumPassContext() = default;
    ~AccumPassContext() = default;
};
}// namespace rbc
RBC_RTTI(rbc::AccumPass)
RBC_RTTI(rbc::AccumPassContext)