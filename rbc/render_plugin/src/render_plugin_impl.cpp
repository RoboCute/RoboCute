#ifdef __clang__
#pragma clang diagnostic ignored "-Winaccessible-base"
#endif
#include <rbc_render/render_plugin.h>
#include <rbc_render/pipeline_context.h>
#include <rbc_render/pt_pipeline.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/compute_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/device_assets/device_image.h>
#ifdef RBC_RENDER_ENABLE_OIDN
#include <oidn_denoiser.h>
#endif
#include <rbc_render/renderer_data.h>
namespace rbc {

#ifdef RBC_RENDER_ENABLE_OIDN
struct DenoiserStream {
    luisa::shared_ptr<Denoiser> denoiser;
    DenoiserStream(
        luisa::shared_ptr<Denoiser> &&denoiser)
        : denoiser(std::move(denoiser)) {
    }
};
#endif

RBC_BIN_2_OBJ_DECLARE(render_settings_json)

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4584)// Disable 'base-class-inheritance' warning
#endif
struct RenderPluginImpl : RenderPlugin, RBCStruct {
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    ////////////////////////////////////////  HDRI
    vstd::optional<HDRI> _hdri;
    vstd::optional<SkyAtmosphere> _sky_atom;
    bool _sky_dirty{false};
    //////////////////////////////////////// denoise
    enum struct OidnSupport : uint8_t {
        UnChecked,
        UnSupported,
        Supported
    };
    OidnSupport _oidn_support{OidnSupport::UnChecked};
    std::mutex _oidn_mtx;
    luisa::shared_ptr<DynamicModule> _oidn_module;
#ifdef RBC_RENDER_ENABLE_OIDN
    rbc::DenoiserExt *_oidn_ext{};
    vstd::HashMap<uint64, DenoiserStream> _denoisers;
#endif
    //////////////////////////////////////// pipeline
    luisa::unordered_map<luisa::string, luisa::unique_ptr<Pipeline>> _pipelines;
    RenderPluginImpl() {
        _pipelines.try_emplace("default", luisa::make_unique<PTPipeline>());
    }
    PipeCtxStub *create_pipeline_context() override {
        auto ctx = new PipelineContext{
            RenderDevice::instance().lc_device(),
            RenderDevice::instance().lc_main_stream(),
            SceneManager::instance(),
            RenderDevice::instance().lc_main_cmd_list()};
        auto default_settings = RBC_BIN_2_OBJ_SPAN(render_settings_json);
        ctx->pipeline_settings.init_json(luisa::string_view((char const *)default_settings.data(), default_settings.size()));
        return reinterpret_cast<PipeCtxStub *>(ctx);
    }
    StateMap *pipe_ctx_state_map(PipeCtxStub *ctx) override {
        return &reinterpret_cast<PipelineContext *>(ctx)->pipeline_settings;
    }
    void destroy_pipeline_context(PipeCtxStub *ctx) override {
        delete reinterpret_cast<PipelineContext *>(ctx);
    }
    Pipeline *get_pipe(luisa::string_view name) const {
        if (name.empty()) {
            name = "default";
        }
        auto iter = _pipelines.find(name);
        if (iter == _pipelines.end()) return nullptr;
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
    void sync_init() override {
        auto ptr = get_pipe({});
        if (!ptr) return;
        ptr->wait_enable();
    }
    bool before_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) override {
        auto ptr = get_pipe(pipeline_name);
        if (!ptr) return false;
        auto &ctx = *reinterpret_cast<PipelineContext *>(pipe_ctx);
        // set sky
        {
            auto &sky_settings = ctx.pipeline_settings.read_mut<SkySettings>();
            sky_settings.sky_atom = _sky_atom.has_value() ? _sky_atom.ptr() : nullptr;
            sky_settings.dirty = _sky_dirty;
            _sky_dirty = false;
        }
        auto &pipe_settings = ctx.pipeline_settings.read<rbc::PTPipelineSettings>();
        if (!pipe_settings.render) return false;
        ptr->wait_enable();
        ptr->early_update(ctx);
        return true;
    }
    bool on_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) override {
        auto ptr = get_pipe(pipeline_name);
        if (!ptr) return false;
        auto &ctx = *reinterpret_cast<PipelineContext *>(pipe_ctx);
        auto &pipe_settings = ctx.pipeline_settings.read<rbc::PTPipelineSettings>();
        if (!pipe_settings.render) return false;
        ptr->update(*reinterpret_cast<PipelineContext *>(pipe_ctx));
        return true;
    }
    void update_skybox(uint2 res) override {
        _sky_dirty = true;
        auto &device = RenderDevice::instance();
        if (!_hdri) {
            _hdri.create();
        }
        if (_sky_atom) {
            _sky_atom->deallocate(SceneManager::instance().bindless_allocator());
            device.lc_main_stream().synchronize();
            _sky_atom.destroy();
        }
        _sky_atom.create(
            device.lc_device(),
            *_hdri,
            res);
    }
    void update_skybox(RC<DeviceImage> image) override {
        _sky_dirty = true;
        auto &device = RenderDevice::instance();
        if (!_hdri) {
            _hdri.create();
        }
        if (_sky_atom) {
            _sky_atom->deallocate(SceneManager::instance().bindless_allocator());
            device.lc_main_stream().synchronize();
            _sky_atom.destroy();
        }
        _sky_atom.create(
            device.lc_device(),
            *_hdri,
            std::move(image));
    }
    bool update_skybox(
        luisa::filesystem::path const &path,
        compute::PixelStorage pixel_storage,
        uint2 resolution,
        uint64_t file_offset_bytes) override {
        _sky_dirty = true;
        auto &device = RenderDevice::instance();
        if (!_hdri) {
            _hdri.create();
        }
        if (!luisa::filesystem::exists(path)) {
            return false;
        }
        IOFile file_stream(luisa::to_string(path));
        if (file_stream.length() - file_offset_bytes < pixel_storage_size(pixel_storage, make_uint3(resolution, 1u))) {
            return false;
        }

        auto img = RC<DeviceImage>::New();
        img->create_texture<float>(
            device.lc_device(),
            pixel_storage, resolution,
            1);
        IOCommandList io_cmdlist;

        io_cmdlist << IOCommand{
            file_stream,
            file_offset_bytes,
            img->get_float_image()};
        io_cmdlist.dispose_file(std::move(file_stream));
        auto load_fence = device.io_service()->execute(std::move(io_cmdlist));
        if (_sky_atom) {
            device.lc_main_stream().synchronize();
            _sky_atom.destroy();
        }
        _sky_atom.create(
            device.lc_device(),
            *_hdri,
            std::move(img));
        device.io_service()->synchronize(load_fence);
        return true;
    }
    void dispose_skybox() override {
        if (_sky_atom) {
            _sky_atom.destroy();
        }
    }
    bool init_oidn() override {
#ifdef RBC_RENDER_ENABLE_OIDN
        std::lock_guard lck{_oidn_mtx};
        if (!ComputeDevice::instance_ptr()) return false;
        auto &render_device = RenderDevice::instance();
        // Unused: auto &lc_ctx = render_device.lc_ctx();
        _oidn_support = ComputeDevice::instance().render_hardware_device_index() == ~0u ? OidnSupport::UnSupported : OidnSupport::Supported;
        if (_oidn_support != OidnSupport::Supported) return false;
        _oidn_module = PluginManager::instance().load_module("oidn_plugin");
        if (!_oidn_module) {
            LUISA_WARNING("OIDN not support for reason: plugin not found.");
            _oidn_support = OidnSupport::UnSupported;
            return false;
        }
        _oidn_ext = _oidn_module->invoke<rbc::DenoiserExt *(luisa::compute::Device const &device)>("rbc_create_oidn", render_device.lc_device());
        if (!_oidn_ext) {
            LUISA_WARNING("OIDN not support for reason: plugin not found.");
            _oidn_support = OidnSupport::UnSupported;
            return false;
        }
        return true;
#else
        LUISA_WARNING("OIDN not enabled in this build.");
        return false;
#endif
    }
    DenoisePack create_denoise_task(
        luisa::compute::Stream &stream,
        PipeCtxStub *ctx,
        uint2 render_resolution) override {
#ifdef RBC_RENDER_ENABLE_OIDN
        if (_oidn_support != OidnSupport::Supported) {
            LUISA_ERROR("Denoiser not supported.");
        }
        bool init = false;

        // emplace denoiser pack
        auto iter = _denoisers.emplace(
            stream.handle(),
            vstd::lazy_eval([&]() {
                init = true;
                return DenoiserStream(_oidn_ext->create());
            }));

        auto &denoiser = *iter.value().denoiser;
        auto &input = reinterpret_cast<PipelineContext *>(ctx)->pipeline_settings.read_mut<DenoiserExt::DenoiserInput>();

        // check if the resolution is changed
        if (input.width != render_resolution.x || input.height != render_resolution.y) {
            init = true;
            input.width = render_resolution.x;
            input.height = render_resolution.y;
        }

        // rebuild denoiser data
        if (init) {
            input.inputs.clear();
            input.outputs.clear();
            input.features.clear();
            input.push_noisy_image(DenoiserExt::ImageFormat::FLOAT3);
            input.push_feature_image("albedo", DenoiserExt::ImageFormat::FLOAT3);
            input.push_feature_image("normal", DenoiserExt::ImageFormat::FLOAT3);
            input.noisy_features = true;
            input.filter_quality = DenoiserExt::FilterQuality::ACCURATE;
            input.prefilter_mode = DenoiserExt::PrefilterMode::ACCURATE;
            denoiser.init(input);
        }

        auto *denoise_ext = static_cast<DXOidnDenoiserExt *>(_oidn_ext);
        return DenoisePack{
            .external_albedo = denoise_ext->buffer_from_image<float>(input.features[0].image),
            .external_normal = denoise_ext->buffer_from_image<float>(input.features[1].image),
            .external_input = denoise_ext->buffer_from_image<float>(input.inputs[0]),
            .external_output = denoise_ext->buffer_from_image<float>(input.outputs[0]),
            .denoise_callback = [&denoiser, &stream]() {
                denoiser.async_execute(stream);
            }};
#else
        return {};
#endif
    }
    void destroy_denoise_task(luisa::compute::Stream &stream) override {
#ifdef RBC_RENDER_ENABLE_OIDN
        _denoisers.remove(stream.handle());
#else
        LUISA_WARNING("OIDN not enabled in this build.");
#endif
    }
    ~RenderPluginImpl() {
        if (_sky_atom)
            _sky_atom->deallocate(SceneManager::instance().bindless_allocator());
#ifdef RBC_RENDER_ENABLE_OIDN
        _denoisers.clear();
#endif
        _pipelines.clear();
        dispose_skybox();
#ifdef RBC_RENDER_ENABLE_OIDN
        delete _oidn_ext;
#endif
    }
};
LUISA_EXPORT_API RenderPlugin *get_render_plugin() {
    static RenderPluginImpl render_plugin_singleton{};
    return &render_plugin_singleton;
}
}// namespace rbc