#include <rbc_render/pipeline_context.h>
#include <rbc_render/renderer_data.h>
namespace rbc
{
PipelineCtxMutable::PipelineCtxMutable()
{
    CameraData cam_data;
    cam_data.world_to_sky = make_float3x3(
    1, 0, 0,
    0, 1, 0,
    0, 0, 1);
    states.write(std::move(cam_data));
}
PipelineCtxMutable::~PipelineCtxMutable() = default;
void PipelineCtxMutable::clear()
{
    _pass_contexts.clear();
}
void PipelineCtxMutable::delay_dispose(DisposeQueue& queue)
{
    if (!_pass_contexts.empty())
    {
        queue.dispose_after_queue(std::move(_pass_contexts));
    }
}

PipelineContext::PipelineContext()
{
}

PipelineContext::PipelineContext(
Device& device,
SceneManager& scene,
CommandList& cmdlist)
    : device{ &device }
    , scene{ &scene }
    , cmdlist{ &cmdlist }
{
}

PipelineContext::~PipelineContext() {}
} // namespace rbc