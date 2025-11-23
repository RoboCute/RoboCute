#include <rbc_runtime/render_plugin.h>
#include <rbc_render/pipeline_context.h>
#include <rbc_render/pt_pipeline.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
struct RenderPluginImpl : RenderPlugin, vstd::IOperatorNewBase {
    // HDRI
    vstd::optional<HDRI> hdri;
    vstd::optional<SkyAtmosphere> sky_atom;

    luisa::unordered_map<luisa::string, luisa::unique_ptr<Pipeline>> pipelines;
    virtual void dispose() override { delete this; }
    RenderPluginImpl() {
        pipelines.try_emplace("default", luisa::make_unique<PTPipeline>());
    }
    PipeCtxStub *create_pipeline_context(StateMap &render_settings_map) override {
        auto ctx = new PipelineContext{
            RenderDevice::instance().lc_device(),
            RenderDevice::instance().lc_main_stream(),
            SceneManager::instance(),
            RenderDevice::instance().lc_main_cmd_list(),
            &render_settings_map};
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
        auto &ctx = *reinterpret_cast<PipelineContext *>(pipe_ctx);
        // set sky
        {
            auto &sky_settings = ctx.pipeline_settings->read_mut<SkySettings>();
            sky_settings.sky_atom = sky_atom.has_value() ? sky_atom.ptr() : nullptr;
        }
        ptr->wait_enable();
        ptr->early_update(ctx);
        return true;
    }
    bool on_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) override {
        auto ptr = get_pipe(pipeline_name);
        if (!ptr) return false;
        ptr->update(*reinterpret_cast<PipelineContext *>(pipe_ctx));
        return true;
    }
    bool update_skybox(
        luisa::filesystem::path const &path,
        uint2 resolution,
        uint64_t file_offset_bytes) override {
        auto &device = RenderDevice::instance();
        if (!hdri) {
            hdri.create();
        }
        if (!luisa::filesystem::exists(path)) {
            return false;
        }
        IOFile file_stream(luisa::to_string(path));
        if (file_stream.length() != resolution.x * resolution.y * sizeof(float4)) {
            return false;
        }
        Image<float> img = device.lc_device().create_image<float>(PixelStorage::FLOAT4, resolution.x, resolution.y);
        IOCommandList io_cmdlist;
        io_cmdlist << IOCommand{
            file_stream,
            file_offset_bytes,
            img.view()};
        io_cmdlist.dispose_file(std::move(file_stream));
        auto load_fence = device.io_service()->execute(std::move(io_cmdlist));
        if (sky_atom) {
            device.lc_main_stream().synchronize();
            sky_atom.destroy();
        }
        sky_atom.create(
            device.lc_device(),
            *hdri,
            std::move(img));
        device.io_service()->synchronize(load_fence);
        return true;
    }
    void dispose_skybox() override {
        auto &device = RenderDevice::instance();
        if (sky_atom) {
            device.lc_main_stream().synchronize();
            sky_atom.destroy();
        }
    }

    ~RenderPluginImpl() {
        pipelines.clear();
        dispose_skybox();
    }
};
RBC_PLUGIN_ENTRY(RenderPlugin) {
    return new RenderPluginImpl();
}
}// namespace rbc