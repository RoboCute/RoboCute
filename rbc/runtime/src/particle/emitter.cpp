/**
 * @file emitter.cpp
 * @brief The Emitter
 * @author sailing-innocent
 * @date 2024-05-18
 */

#include "rbc_particle/emitter.h"
#include "rbc_particle/system.h"
#include "rbc_particle/resource/shader_manager.h"

#include "luisa/ast/function_builder.h"

#include "rbc_particle/ir/parser.h"
#include "rbc_particle/ir/module.h"
#include "rbc_particle/ir/func_builder.h"
// Core Impl
namespace rbc::ps {

class ParticleEmitter::ParticleEmitterImpl {
    float m_lifetime = 1.0f;
    int m_capacity = 1000;
    vstd::HashMap<luisa::string, BufferInstance> m_buffers = {};

public:
    ParticleEmitterImpl() {};
    ~ParticleEmitterImpl() {};
    // remove copy
    ParticleEmitterImpl(const ParticleEmitterImpl &) = delete;
    ParticleEmitterImpl &operator=(const ParticleEmitterImpl &) = delete;
    // default move
    ParticleEmitterImpl(ParticleEmitterImpl &&) noexcept {};
    ParticleEmitterImpl &operator=(ParticleEmitterImpl &&) {};

    void set_life_time(float life_time) {
        LUISA_ASSERT(life_time > 0.0f, "life time should be positive");
        m_lifetime = life_time;
    }
    void set_capacity(int capacity) {
        LUISA_ASSERT(capacity > 0, "capacity should be positive");
        m_capacity = capacity;
    }
    [[nodiscard]] float get_life_time() const { return m_lifetime; }
    [[nodiscard]] int get_capacity() const { return m_capacity; }

    [[nodiscard]] int get_spawn_size(float curr_time) {
        if (curr_time <= 0.0f) { return 0; }
        if (curr_time >= m_lifetime) { return m_capacity; }
        return int(float(m_capacity) * curr_time / m_lifetime);
    }

    void emplace_buffer_inst(luisa::string name, BufferInstance &&buf_inst) noexcept {
        m_buffers.emplace(name, std::move(buf_inst));
    }

    [[nodiscard]] BufferInstance &get_buffer_inst(const luisa::string &name) noexcept {
        auto iter = m_buffers.find(name);
        if (!iter) {
            LUISA_ERROR("buffer not found");
        }
        return iter.value();
    }
};

}// namespace rbc::ps

// API
namespace rbc::ps {

ParticleEmitter::ParticleEmitter(ParticleSystem &system, EmitterConfig &&config) noexcept
    : m_system(system), m_config(std::move(config)), mp_impl(std::move(eastl::make_unique<ParticleEmitterImpl>())) {
    mp_update_state_proxy = eastl::make_unique<EmitterUpdateStateProxy>();
    mp_render_state_proxy = eastl::make_unique<EmitterRenderStateProxy>();
    mp_impl->set_capacity(config.capacity);
    mp_impl->set_life_time(config.life_time);
}
ParticleEmitter::~ParticleEmitter() noexcept {}

void ParticleEmitter::allocate_buffer(luisa::string name, const Type *p_type, int align, bool add_update, bool add_view) noexcept {
    BufferInstance buffer_instance{
        .buffer = m_system.device().create_buffer<uint>(m_config.capacity * align),
        .p_type = Type::of(Buffer<float>())};
    mp_impl->emplace_buffer_inst(name, std::move(buffer_instance));
    auto &buf_inst = mp_impl->get_buffer_inst(name);

    LUISA_INFO("allocate buffer {}, handle {}", name, buf_inst.buffer.handle());
    if (add_update) {
        BufferProxy buf_proxy{
            .uint_view = buf_inst.buffer.view(),
            .p_type = buf_inst.p_type};
        mp_update_state_proxy->buffers.emplace(name, buf_proxy);
    }
    if (add_view) {
        BufferProxy buf_proxy{
            .uint_view = buf_inst.buffer.view(),
            .p_type = buf_inst.p_type};
        mp_render_state_proxy->buffers.emplace(name, buf_proxy);
    }
}
void ParticleEmitter::create() {
    m_system.ir_parser().load_file(
        "D:/repos/D5-NEXT/build/simple.pir");
    auto ir{m_system.ir_parser().parse()};
    // TODO: allocate and encode params based on IR
    // allocate buffer
    allocate_buffer("pos", Type::of<float>(), 3, true, true);
    allocate_buffer("vel", Type::of<float>(), 3, true, false);

    // register render
    m_system.register_emitter_render(*mp_render_state_proxy);
    auto shad_ptr = m_system.shader_manager().emplace_shader({"dummy_shader"});
    // encode shader call
    shad_ptr->compile(m_system.device(), m_system.ir_func_builder().build_func(*ir)->function());
    m_shader_pipeline.emplace_back(shad_ptr);
    auto &shader_call = m_shader_pipeline.back();
    ShaderArgument arg0{ShaderArgument::ArgumentTag::BUFFER, "pos"};
    ShaderArgument arg1{ShaderArgument::ArgumentTag::BUFFER, "vel"};
    shader_call.emplace_argument(std::move(arg0));
    shader_call.emplace_argument(std::move(arg1));
}
void ParticleEmitter::init(CommandList &cmdlist) noexcept {
    luisa::vector<float> h_pos(m_config.capacity * 3);
    for (size_t i = 0; i < m_config.capacity; i++) {
        auto theta = float(i) / float(m_config.capacity) * 2.0f * 3.1415926f;
        h_pos[i * 3 + 0] = 0.5f * std::cos(theta);
        h_pos[i * 3 + 1] = 0.5f * std::sin(theta);
        h_pos[i * 3 + 2] = 0.0f;
    }
    auto iter = mp_update_state_proxy->buffers.find("pos");
    if (!iter) {
        LUISA_ERROR("buffer not found");
        return;
    }
    auto buf_view = iter.value().uint_view.as<float>();
    cmdlist << buf_view.copy_from(h_pos.data());

    luisa::vector<float> h_vel(m_config.capacity * 3, 0.0f);
    iter = mp_update_state_proxy->buffers.find("vel");
    if (!iter) {
        LUISA_ERROR("vel buffer not found");
        return;
    }
    buf_view = iter.value().uint_view.as<float>();
    cmdlist << buf_view.copy_from(h_vel.data());

    m_is_active = true;
    m_clock.tic();
}

void ParticleEmitter::pause() { m_is_active = false; }
void ParticleEmitter::resume() { m_is_active = true; }

void ParticleEmitter::before_update() {
    // encode shader call
    mp_update_state_proxy->view_size = m_view_size;
}

void ParticleEmitter::update(CommandList &cmdlist) noexcept {
    // update view size
    auto curr_time = m_clock.toc();
    if (!m_is_active) {
        m_paused_time += curr_time - m_last_update_time;
        m_last_update_time = curr_time;
        return;
    }
    m_view_size = mp_impl->get_spawn_size(curr_time - m_paused_time);
    m_last_update_time = curr_time;

    // cmdlist << shader_calls
    auto dispatch_size = make_uint3(mp_update_state_proxy->view_size, 1, 1);
    if (mp_update_state_proxy->view_size > 0) {
        for (auto &shader_call : m_shader_pipeline) {
            shader_call.create_encoder();
            for (auto &arg : shader_call.arguments) {
                switch (arg.tag) {
                    case ShaderArgument::ArgumentTag::BUFFER: {
                        auto buf_view = mp_update_state_proxy->get_buffer(arg.name).uint_view;

                        shader_call.encode_buffer(buf_view.handle(), buf_view.offset_bytes(), buf_view.size_bytes());
                        break;
                    }
                    default:
                        LUISA_ERROR("unknown argument tag");
                }
            }
            shader_call.set_ready(true);
            if (shader_call.is_ready()) {
                cmdlist << std::move(shader_call).dispatch(dispatch_size);
            }
            if (shader_call.is_sync()) {
                // [TODO]: sync when update?
            }
        }
    }
    // LUISA_INFO("update view size {}", mp_update_state_proxy->view_size);
}

void ParticleEmitter::before_render() {
    mp_render_state_proxy->view_size = m_view_size;
}

void ParticleEmitter::destroy() {
    // [TODO] destroy buffers
}

}// namespace rbc::ps