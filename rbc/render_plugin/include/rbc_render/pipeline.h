#pragma once
#include <rbc_config.h>
#include <rbc_core/state_map.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/event.h>
#include <rbc_render/pass.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_core/type_info.h>

namespace rbc {
struct PipelineController;
struct RenderDevice;

struct Pipeline {
public:
    virtual void update(PipelineContext &ctx);
    virtual void early_update(PipelineContext &ctx);

private:
    vstd::vector<vstd::unique_ptr<Pass>> _passes;
    vstd::HashMap<TypeInfo, size_t> _pass_indices;
    bool initialized = false;

public:
    virtual void initialize() {}
    // mutable vstd::LMDB db;
    Pass *_get_pass(TypeInfo const &name) const;
    template<typename T>
        requires(std::is_base_of_v<Pass, T>)
    T *get_pass() const {
        return static_cast<T *>(_get_pass(TypeInfo::get()));
    }

    Pipeline();
    virtual ~Pipeline();
    Pass *_emplace_instance(vstd::unique_ptr<Pass> &&component, TypeInfo const &name, bool init_enabled = true);
    template<typename T, typename... Args>
        requires(std::is_base_of_v<Pass, T> && std::is_constructible_v<T, Args && ...>)
    T *emplace_instance(Args &&...args) {
        return static_cast<T *>(_emplace_instance(vstd::unique_ptr<Pass>(new T{std::forward<Args>(args)...}), TypeInfo::get<T>()));
    }
    template<typename T, typename... Args>
        requires(std::is_base_of_v<Pass, T> && std::is_constructible_v<T, Args && ...>)
    T *emplace_disabled_instance(Args &&...args) {
        return static_cast<T *>(_emplace_instance(vstd::unique_ptr<Pass>(new T{std::forward<Args>(args)...}), TypeInfo::get<T>(), false));
    }
    void enable(
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene);

    void wait_enable();

    void disable(
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene);
};
}// namespace rbc