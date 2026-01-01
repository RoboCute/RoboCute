#pragma once
/**
 * The Virtual Machine that execute the Particle IR
 */

#include <luisa/vstl/common.h>
#include "rbc_particle/resource/buffer.h"
#include "rbc_particle/resource/shader.h"
#include "rbc_particle/system.h"

namespace rbc::ps {

enum struct EmitType {
    LINEAR,
    ONCE
};
struct EmitterConfig {
    luisa::string name = "DefaultEmitter";
    size_t capacity = 1000;
    float life_time = 30000.0f;
    EmitType emit_type = EmitType::LINEAR;
};

// Proxy
struct EmitterStateProxy {
    vstd::HashMap<luisa::string, BufferProxy> buffers;
    size_t view_size = 0;
    luisa::string_view name;

    EmitterStateProxy() noexcept = default;
    // delete copy
    EmitterStateProxy(const EmitterStateProxy &) = delete;
    EmitterStateProxy &operator=(const EmitterStateProxy &) = delete;
    // default move
    EmitterStateProxy(EmitterStateProxy &&) noexcept = default;
    EmitterStateProxy &operator=(EmitterStateProxy &&) noexcept = default;
    virtual ~EmitterStateProxy() noexcept {
        buffers.clear();
    };
    [[nodiscard]] BufferProxy &get_buffer(const luisa::string &name) noexcept {
        auto iter = buffers.find(name);
        if (!iter) {
            LUISA_ERROR("buffer not found");
        }
        return iter.value();
    }
};

struct EmitterUpdateStateProxy : public EmitterStateProxy {
};
struct EmitterRenderStateProxy : public EmitterStateProxy {
    luisa::string pass_name = "splat";
};

class ParticleEmitter {

    ParticleSystem &m_system;
    // proxy for the buffer
    eastl::unique_ptr<EmitterUpdateStateProxy> mp_update_state_proxy;
    eastl::unique_ptr<EmitterRenderStateProxy> mp_render_state_proxy;

    bool m_is_visible = true;// if false, the emitter will not be rendered
    bool m_is_active = false;// if false, the emitter will not be updated
    size_t m_view_size = 0;

    // internal paused time
    float m_paused_time = 0.0f;
    float m_last_update_time = 0.0f;
    luisa::Clock m_clock;

    EmitterConfig m_config;
    // pipeline is a list of shader call
    luisa::vector<ShaderCall> m_shader_pipeline;

private:
    class ParticleEmitterImpl;
    eastl::unique_ptr<ParticleEmitterImpl> mp_impl;

public:
    ParticleEmitter(ParticleSystem &system, EmitterConfig &&config = {}) noexcept;
    // delete copy
    ParticleEmitter(const ParticleEmitter &) = delete;
    ParticleEmitter &operator=(const ParticleEmitter &) = delete;
    // default move
    ParticleEmitter(ParticleEmitter &&) noexcept = default;
    ParticleEmitter &operator=(ParticleEmitter &&) noexcept = default;
    ~ParticleEmitter() noexcept;
    void pause();
    void resume();

    // life cycle
    void create();
    void init(CommandList &cmdlist) noexcept;// initial value for the buffer and state
    void before_update();                    // update the update state proxy
    void update(CommandList &cmdlist) noexcept;
    void before_render();// update the render target proxy
    // render tick is not managed by the emitter
    void destroy();

    // getter
    [[nodiscard]] bool is_active() const { return m_is_active; }

private:
    void allocate_buffer(luisa::string name, const Type *p_type, int align = 1, bool add_update = true, bool add_view = true) noexcept;
};

}// namespace rbc::ps