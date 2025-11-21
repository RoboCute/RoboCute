#pragma once
#include <rbc_config.h>
#include <rbc_core/state_map.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/event.h>
#include <rbc_render/pass.h>
#include <rbc_render/generated/pipeline_settings.hpp>

namespace rbc {
struct PipelineController;
struct RenderDevice;

struct Pipeline {
public:
    virtual bool initialize(RenderDevice *device, PipelineContext &ctx) {
        _render_device = device;
        return true;
    }
    // 上下文是一个黑盒，包含渲染管线运行需要的内部状态，每帧创建一个，不可复制。
    virtual PipelineContext create_context();
    virtual void update(PipelineContext &ctx);
    virtual void early_update(PipelineContext &ctx);

private:
    vstd::string _name;
    vstd::vector<vstd::unique_ptr<Pass>> _passes;
    vstd::HashMap<TypeInfo, size_t> _pass_indices;
    vstd::vector<Pass *> _enabled_passes;
    bool initialized = false;

public:
    // mutable vstd::LMDB db;
    Pass *_get_pass(TypeInfo const &name) const;
    [[nodiscard]] auto name() const { return vstd::string_view(_name); }
    template<typename T>
        requires(std::is_base_of_v<Pass, T>)
    T *get_pass() const {
        return static_cast<T *>(_get_pass(TypeInfo::get()));
    }

    Pipeline(vstd::string name);
    virtual ~Pipeline();
    Pass *_emplace_instance(vstd::unique_ptr<Pass> &&component, TypeInfo const &name, bool init_enabled = true);
    template<typename T, typename... Args>
        requires(std::is_base_of_v<Pass, T> && std::is_constructible_v<T, Args && ...>)
    T *emplace_instance(Args &&...args) {
        return static_cast<T *>(_emplace_instance(vstd::unique_ptr<Pass>(new T{std::forward<Args>(args)...}), TypeInfo::get()));
    }
    template<typename T, typename... Args>
        requires(std::is_base_of_v<Pass, T> && std::is_constructible_v<T, Args && ...>)
    T *emplace_disabled_instance(Args &&...args) {
        return static_cast<T *>(_emplace_instance(vstd::unique_ptr<Pass>(new T{std::forward<Args>(args)...}), TypeInfo::get(), false));
    }
    void enable(
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene);

    void wait_enable();

    void refresh(PipelineContext &ctx, Stream &stream);
    void disable(
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene);

protected:
    RenderDevice *_render_device;
};
}// namespace rbc