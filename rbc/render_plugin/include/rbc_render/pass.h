#pragma once
#include <rbc_config.h>
#include <rbc_render/pipeline_context.h>
#include <luisa/vstl/common.h>
namespace rbc {
struct Pipeline;
struct Pass : public vstd::IOperatorNewBase {
	friend struct Pipeline;

private:
	bool _actived = false;

public:
	[[nodiscard]] auto actived() const { return _actived; }
	void set_actived(bool value) { _actived = value; }
	// called when pipeline is enabled
	virtual void on_enable(
		Pipeline const& pipeline,
		Device& device,
		CommandList& cmdlist,
		SceneManager& scene) = 0;
	virtual void wait_enable() {}
	// called per frame before all update and frame-update task like accel, bindless, light-bvh, etc..
	virtual void early_update(Pipeline const& pipeline, PipelineContext const& ctx) {}
	// called per frame
	virtual void update(Pipeline const& pipeline, PipelineContext const& ctx) = 0;
	// called per frame after all update
	virtual void on_frame_end(
		Pipeline const& pipeline,
		Device& device,
		SceneManager& scene) {}
	// called when pipeline is disabled
	virtual void on_disable(
		Pipeline const& pipeline,
		Device& device,
		CommandList& cmdlist,
		SceneManager& scene) = 0;
	Pass() = default;
	Pass& operator=(Pass const&) = delete;
	Pass& operator=(Pass&&) = delete;
	Pass(Pass const&) = delete;
	Pass(Pass&&) = delete;
	virtual ~Pass() = default;
};
}// namespace rbc