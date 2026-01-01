#pragma once
/**
 * @file system.h
 * @brief Management for Emitters, Scene, IO, compiler, lifecycle etc. for Particle System
 * @author sailing-innocent
 * @date 2024-05-16
 */
#include "rbc_config.h"
#include "rbc_particle/scene/scene.h"
#include "rbc_particle/emitter.h"

#include <luisa/luisa-compute.h>
#include <EASTL/unique_ptr.h>

namespace rbc::ps {

class PSPipelineCtxMutable;
struct EmitterConfig;
class ShaderManager;
class IRFuncBuilder;
class IRParser;

class ParticleSystem {
    friend class ParticleEmitter;
    // reference
    Context &mr_ctx;
    Device &mr_device;

    // components
    eastl::unique_ptr<IRParser> mp_ir_parser;
    eastl::unique_ptr<IRFuncBuilder> mp_ir_func_builder;

    eastl::unique_ptr<ShaderManager> mp_shader_manager;
    eastl::unique_ptr<PSPipelineCtxMutable> mp_render_ctx_mut;
    eastl::unique_ptr<Scene> mp_scene;
    eastl::vector<eastl::unique_ptr<ParticleEmitter>> m_emitters;

public:
    struct Config {
        [[nodiscard]] static Config make_default() { return {}; }
    };
    explicit ParticleSystem(Context &ctx, Device &device, luisa::filesystem::path const &shader_path) noexcept;
    // delete copy and move
    ParticleSystem(ParticleSystem const &) = delete;
    ParticleSystem &operator=(ParticleSystem const &) = delete;
    ParticleSystem(ParticleSystem &&) = delete;
    ParticleSystem &operator=(ParticleSystem &&) = delete;
    ~ParticleSystem();

    // lifecycle
    void create_emitter_instance(EmitterConfig const &config) noexcept;

    void create() noexcept;
    void update(CommandList &cmdlist) noexcept;
    void before_render() noexcept;
    void pause() noexcept;
    void resume() noexcept;

    // getter

    [[nodiscard]] Context &context() noexcept { return mr_ctx; }
    [[nodiscard]] Device &device() noexcept { return mr_device; }

    [[nodiscard]] Scene &scene() noexcept { return *mp_scene; }
    [[nodiscard]] PSPipelineCtxMutable &render_ctx_mut() noexcept { return *mp_render_ctx_mut; }
    [[nodiscard]] ShaderManager &shader_manager() noexcept { return *mp_shader_manager; }

    [[nodiscard]] IRParser &ir_parser() noexcept { return *mp_ir_parser; }
    [[nodiscard]] IRFuncBuilder &ir_func_builder() noexcept { return *mp_ir_func_builder; }

private:
    bool register_emitter_render(EmitterRenderStateProxy const &render_state_proxy) noexcept;
};

}// namespace rbc::ps