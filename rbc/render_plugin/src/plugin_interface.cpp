#include <rbc_runtime/render_plugin.h>
#include <rbc_render/pipeline_context.h>
#include <rbc_render/pt_pipeline.h>
#include <rbc_runtime/plugin_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <oidn_denoiser.h>
namespace rbc {

struct DenoiserStream {
    luisa::shared_ptr<Denoiser> denoiser;
    DenoiserExt::DenoiserInput input;
    DenoiserStream(
        luisa::shared_ptr<Denoiser> &&denoiser,
        uint2 res)
        : denoiser(std::move(denoiser)), input(res.x, res.y) {
    }
};

struct RenderPluginImpl : RenderPlugin, vstd::IOperatorNewBase {
    ////////////////////////////////////////  HDRI
    vstd::optional<HDRI> hdri;
    vstd::optional<SkyAtmosphere> sky_atom;
    //////////////////////////////////////// denoise
    enum struct OidnSupport : uint8_t {
        UnChecked,
        UnSupported,
        Supported
    };
    OidnSupport oidn_support{OidnSupport::UnChecked};
    std::mutex oidn_mtx;
    luisa::shared_ptr<DynamicModule> oidn_module;
    rbc::DenoiserExt *oidn_ext{};
    vstd::HashMap<uint64, DenoiserStream> _denoisers;
    //////////////////////////////////////// pipeline
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
    void clear_context(PipeCtxStub *ctx) override {
        reinterpret_cast<PipelineContext *>(ctx)->clear();
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
            sky_atom.destroy();
        }
    }
    bool init_oidn() override {
        std::lock_guard lck{oidn_mtx};
        auto &render_device = RenderDevice::instance();
        auto &lc_ctx = render_device.lc_ctx();
        if (oidn_support == OidnSupport::UnChecked) {
            auto checker_path = luisa::to_string(lc_ctx.runtime_directory() / "oidn_checker.exe");
            checker_path = luisa::format("{} {} {}", checker_path, luisa::to_string(lc_ctx.runtime_directory()), render_device.backend_name());
            auto result = std::system(checker_path.c_str());
            oidn_support = OidnSupport::UnSupported;
            switch (result) {
                case 0:
                    LUISA_INFO("OIDN support.");
                    oidn_support = OidnSupport::Supported;
                    break;
                case 1:
                    LUISA_WARNING("OIDN not support for reason: invalid check args.");
                    break;
                case 2:
                    LUISA_WARNING("OIDN not support for reason: unsupported backend.");
                    break;
                case 3:
                    LUISA_WARNING("OIDN not support for reason: plugin not found.");
                    break;
                default:
                    LUISA_WARNING("OIDN not support for unknown reason.");
                    break;
            }
        }
        if (oidn_support != OidnSupport::Supported) return false;
        oidn_module = PluginManager::instance().load_module("oidn_plugin");
        if (!oidn_module) {
            LUISA_WARNING("OIDN not support for reason: plugin not found.");
            oidn_support != OidnSupport::UnSupported;
            return false;
        }
        oidn_ext = oidn_module->invoke<rbc::DenoiserExt *(luisa::compute::Device const &device)>("rbc_create_oidn", render_device.lc_device());
        if (!oidn_ext) {
            LUISA_WARNING("OIDN not support for reason: plugin not found.");
            oidn_support != OidnSupport::UnSupported;
            return false;
        }
        return true;
    }
    DenoisePack create_denoise_task(
        luisa::compute::Stream &stream,
        uint2 render_resolution) override {
        if (oidn_support != OidnSupport::Supported) {
            LUISA_ERROR("Denoiser not supported.");
        }
        bool init = false;

        // emplace denoiser pack
        auto iter = _denoisers.emplace(
            stream.handle(),
            vstd::lazy_eval([&]() {
                init = true;
                return DenoiserStream(oidn_ext->create(), render_resolution);
            }));

        auto &denoiser = *iter.value().denoiser;
        auto &input = iter.value().input;

        // check if the resolution is changed
        if (input.width != render_resolution.x || input.height != render_resolution.y) {
            init = true;
            vstd::reset(input, render_resolution.x, render_resolution.y);
        }

        // rebuild denoiser data
        if (init) {
            input.push_noisy_image(DenoiserExt::ImageFormat::FLOAT3);
            input.push_feature_image("albedo", DenoiserExt::ImageFormat::FLOAT3);
            input.push_feature_image("normal", DenoiserExt::ImageFormat::FLOAT3);
            input.noisy_features = true;
            input.filter_quality = DenoiserExt::FilterQuality::ACCURATE;
            input.prefilter_mode = DenoiserExt::PrefilterMode::ACCURATE;
            denoiser.init(input);
        }

        auto *denoise_ext = static_cast<DXOidnDenoiserExt *>(oidn_ext);
        return DenoisePack{
            .external_albedo = denoise_ext->buffer_from_image<float>(input.features[0].image),
            .external_normal = denoise_ext->buffer_from_image<float>(input.features[1].image),
            .external_input = denoise_ext->buffer_from_image<float>(input.inputs[0]),
            .external_output = denoise_ext->buffer_from_image<float>(input.outputs[0]),
            .denoise_callback = [&denoiser, &stream]() {
                stream << synchronize();
                denoiser.execute();
                stream << synchronize();
            }};
    }
    void destroy_denoise_task(luisa::compute::Stream &stream) override {
        _denoisers.remove(stream.handle());
    }
    ~RenderPluginImpl() {
        _denoisers.clear();
        pipelines.clear();
        dispose_skybox();
        delete oidn_ext;
    }
};
RBC_PLUGIN_ENTRY(RenderPlugin) {
    return new RenderPluginImpl();
}
}// namespace rbc