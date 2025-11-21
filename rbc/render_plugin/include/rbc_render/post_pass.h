#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/shader.h>
#include <rbc_render/post_process/lpm.h>
#include <rbc_render/post_process/aces.h>
#include <rbc_render/post_process/exposure.h>
#include <rbc_render/generated/pipeline_settings.hpp>
namespace post_uber_pass {
using namespace luisa;
template<typename T, size_t i>
using Array = std::array<T, i>;
#include <post_process/uber_arg.inl>
}// namespace post_uber_pass
namespace rbc {
struct BufferUploader;
struct PostPassContext;
struct  PostPass : public Pass {
public:
	vstd::optional<LPM> lpm;
	DeviceConfigExt* device_config;
	// Shader2D<Image<float>, Image<float>> const* hdr_to_ldr_shader;

private:
	PostPassContext* post_ctx{};
	bool aces_lut_dirty = true;

	using UberShader = Shader2D<
		Image<float>, // src_img,
		Volume<float>,// tonemap_volume,
		post_uber_pass::Args,
		Buffer<float>,// exposure buffer
		Image<float>  // out texture
		>;

	using UberLpmShader = Shader2D<
		Image<float>, // src_img,
		Volume<float>,// tonemap_volume,
		post_uber_pass::Args,
		post_uber_pass::LpmArgs,
		Buffer<float>,// exposure buffer
		Image<float>  // out texture
		>;
	UberShader const* uber_shader{};
	UberLpmShader const* uber_lpm_shader{};
	bool _use_lpm = false;

public:
	BufferView<float> exposure_buffer() const;

	PostPass(DeviceConfigExt* device_config = nullptr);

	~PostPass();

	void on_enable(Pipeline const& pipeline, Device& device, CommandList& cmdlist, SceneManager& scene) override;

	void early_update(Pipeline const& pipeline, PipelineContext const& ctx) override;

	void update(Pipeline const& pipeline, PipelineContext const& ctx) override;

	void on_frame_end(Pipeline const& pipeline, Device& device, SceneManager& scene) override;

	void on_disable(Pipeline const& pipeline, Device& device, CommandList& cmdlist, SceneManager& scene) override;
};
}// namespace rbc
RBC_RTTI(rbc::PostPass)