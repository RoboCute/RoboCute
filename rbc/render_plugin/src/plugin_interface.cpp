#include <rbc_runtime/render_plugin.h>
#include <rbc_render/pipeline_context.h>
#include <rbc_render/pt_pipeline.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
struct RenderPluginImpl : RenderPlugin {
    luisa::unordered_map<luisa::string, luisa::unique_ptr<Pipeline>> pipelines;

    RenderPluginImpl() {
        pipelines.try_emplace("default", luisa::make_unique<PTPipeline>());
    }
    PipeCtxStub *create_pipeline_context(StateMap &render_settings_map) override {
        auto ctx = new PipelineContext{};
        ctx->pipeline_settings = &render_settings_map;
        ctx->device = &RenderDevice::instance().lc_device();
        ctx->scene = &SceneManager::instance();
        ctx->cmdlist = &RenderDevice::instance().lc_main_cmd_list();
        return reinterpret_cast<PipeCtxStub *>(ctx);
    }
    void destroy_pipeline_context(PipeCtxStub *ctx) override {
        delete reinterpret_cast<PipelineContext *>(ctx);
    }
    Camera &get_camera(PipeCtxStub *pipe_ctx) override {
        return reinterpret_cast<PipelineContext *>(pipe_ctx)->cam;
    }
    Pipeline *get_pipe(luisa::string_view name) {
        if (name.empty()) {
            name = "default";
        }
        auto iter = pipelines.find(name);
        if (iter == pipelines.end()) return nullptr;
        return iter->second.get();
    }

    // render_loop
    bool initialize_pipeline(luisa::string_view pipeline_name) override {
        auto ptr = get_pipe(pipeline_name);
        if (!ptr) return false;
        ptr->initialize();
        return true;
    }
    bool before_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) override {
        auto ptr = get_pipe(pipeline_name);
        if (!ptr) return false;
        ptr->early_update(*reinterpret_cast<PipelineContext *>(pipe_ctx));
        return true;
    }
    bool on_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) override {
        auto ptr = get_pipe(pipeline_name);
        if (!ptr) return false;
        ptr->update(*reinterpret_cast<PipelineContext *>(pipe_ctx));
        return true;
    }
};
RBC_LOAD_PLUGIN_ENTRY(RenderPlugin) {
    return new RenderPluginImpl();
}
}// namespace rbc