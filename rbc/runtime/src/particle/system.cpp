#include "rbc_particle/system.h"
#include "rbc_particle/emitter.h"
#include "rbc_particle/scene/scene.h"
#include "rbc_particle/render/pipeline_context.h"
#include "rbc_particle/render/pass/splat.h"
#include "rbc_particle/resource/shader_manager.h"
#include "rbc_particle/ir/parser.h"
#include "rbc_particle/ir/func_builder.h"

// interface
namespace rbc::ps {

ParticleSystem::ParticleSystem(Context &ctx, Device &device, luisa::filesystem::path const &shader_path) noexcept
    : mr_ctx(ctx), mr_device(device) {
    mp_scene = eastl::make_unique<Scene>();
    mp_render_ctx_mut = eastl::make_unique<PSPipelineCtxMutable>();
    // add splat context
    mp_render_ctx_mut->get_pass_context<SplatPSPassContext>("splat");
    // add shader manager
    mp_shader_manager = eastl::make_unique<ShaderManager>();

    mp_ir_parser = eastl::make_unique<IRParser>();
    mp_ir_func_builder = eastl::make_unique<IRFuncBuilder>();
}

ParticleSystem::~ParticleSystem() {}

// lifecycle
void ParticleSystem::create() noexcept {
    for (auto &emitter : m_emitters) {
        emitter->create();
    }
}
void ParticleSystem::create_emitter_instance(EmitterConfig const &config) noexcept {
    m_emitters.emplace_back(eastl::make_unique<ParticleEmitter>(*this, EmitterConfig{config}));
}
bool ParticleSystem::register_emitter_render(EmitterRenderStateProxy const &render_state_proxy) noexcept {
    LUISA_INFO("register_emitter_render");
    // [TODO] add render state proxy into scene
    auto idx = mp_scene->emplace_emitter_node(render_state_proxy);
    auto *p_pass_ctx = mp_render_ctx_mut->get_pass_context<SplatPSPassContext>(render_state_proxy.pass_name);
    // add binding
    p_pass_ctx->emitter_scene_node_indices.push_back(idx);
    LUISA_INFO("register emitter scene node {} to {} render", idx, render_state_proxy.pass_name);
    return true;
}

void ParticleSystem::update(CommandList &cmdlist) noexcept {
    for (auto &p_emitter : m_emitters) {
        // if not active, init once
        if (!p_emitter->is_active()) {
            p_emitter->init(cmdlist);
        }
        p_emitter->before_update();
        p_emitter->update(cmdlist);
    }
}

void ParticleSystem::before_render() noexcept {
    for (auto &p_emitter : m_emitters) {
        p_emitter->before_render();
    }
}

void ParticleSystem::pause() noexcept {
    for (auto &p_emitter : m_emitters) {
        p_emitter->pause();
    }
}
void ParticleSystem::resume() noexcept {
    for (auto &p_emitter : m_emitters) {
        p_emitter->resume();
    }
}

}// namespace rbc::ps