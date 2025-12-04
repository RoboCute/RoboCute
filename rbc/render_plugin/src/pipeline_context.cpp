#include <rbc_render/pipeline_context.h>
#include <rbc_render/renderer_data.h>
#include <rbc_render/click_manager.h>
namespace rbc {
PipelineCtxMutable::PipelineCtxMutable() {
}
PipelineCtxMutable::~PipelineCtxMutable() = default;
void PipelineCtxMutable::clear() {
    _pass_contexts.clear();
}
void PipelineCtxMutable::delay_dispose(DisposeQueue &queue) {
    if (!_pass_contexts.empty()) {
        queue.dispose_after_queue(std::move(_pass_contexts));
    }
}

PipelineContext::PipelineContext() {
}

PipelineContext::PipelineContext(
    Device &device,
    Stream &stream,
    SceneManager &scene,
    CommandList &cmdlist,
    rbc::StateMap *pipeline_settings)
    : device{&device}, stream(&stream), scene{&scene}, cmdlist{&cmdlist}, pipeline_settings(pipeline_settings) {
    auto &cam_data = pipeline_settings->read_mut<CameraData>();
    cam_data.world_to_sky = make_float3x3(
        1, 0, 0,
        0, 1, 0,
        0, 0, 1);
}
void PipelineContext::clear() {
    mut.clear();
    auto click_mng = pipeline_settings->read_if<ClickManager>();
    if (click_mng)
        click_mng->clear();
}

PipelineContext::~PipelineContext() {}
}// namespace rbc