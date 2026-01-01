#pragma once
/**
 * @file pipeline_context.h
 * @brief The Pipeline Context for Particle System
 * @author sailing-innocent
 * @date 2024-05-16
 */
#include <luisa/runtime/image.h>
#include <rbc_particle/scene/scene.h>


namespace rbc::ps
{

class ParticleSystem;
struct PSPassContext {
    virtual ~PSPassContext() = default;
};

class PSPipelineCtxMutable
{
    vstd::HashMap<luisa::string, luisa::unique_ptr<PSPassContext>> m_pass_contexts;

public:
    template <typename T>
    requires(std::is_base_of_v<PSPassContext, T> && luisa::is_constructible_v<T>)
    T* get_pass_context(luisa::string_view name)
    {
        return static_cast<T*>(
        m_pass_contexts.emplace(
                       name,
                       vstd::lazy_eval([&]() -> eastl::unique_ptr<PSPassContext> {
                           return eastl::make_unique<T>();
                       }))
        .value()
        .get());
    }
};

struct PSPipelineContext {
    CommandList&        cmdlist;
    Device&             device;
    Scene&              scene;
    uint2               resolution;
    Image<float> const* dst_img;
    // TODO: change mutable to support multi-thread
    PSPipelineCtxMutable& mut;
    // support move
};

} // namespace rbc::ps